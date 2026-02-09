import rclpy
from rclpy.node import Node
from sensor_msgs.msg import LaserScan
import serial
import math
from geometry_msgs.msg import TransformStamped
from tf2_ros import StaticTransformBroadcaster

# ---------------------------------------------------------------------------
# LD06 protocol constants
# ---------------------------------------------------------------------------
FRAME_LEN       = 47
HEADER_BYTE_0   = 0x54
HEADER_BYTE_1   = 0x2C
POINTS_PER_FRAME = 12

# ---------------------------------------------------------------------------
# Scan-grid constants  (uniform 0.5 ° bins covering 0 … 359.5 °)
# ---------------------------------------------------------------------------
NUM_BINS        = 360                          # 360 / 1.0
BIN_SIZE_DEG    = 360.0 / NUM_BINS             # 1.0 °
DEG2RAD         = math.pi / 180.0

# ROS LaserScan output: -π … +π  (bin 0 = 0 ° → mapped to 0 rad,
# but final arrays are rotated so index 0 = -π)
ANGLE_MIN       = -math.pi
ANGLE_MAX       =  math.pi - (2.0 * math.pi / NUM_BINS)   # angle of the *last* bin
ANGLE_INCREMENT = (2.0 * math.pi) / NUM_BINS               # uniform step, ~0.00873 rad


# ---------------------------------------------------------------------------
# CRC-16 (polynomial 0xA001, initial value 0x0000) — used by LD06
# ---------------------------------------------------------------------------
def _build_crc_table():
    table = [0] * 256
    for i in range(256):
        crc = i
        for _ in range(8):
            if crc & 1:
                crc = (crc >> 1) ^ 0xA001
            else:
                crc >>= 1
        table[i] = crc
    return table

_CRC_TABLE = _build_crc_table()


def crc16(data: bytes) -> int:
    """Compute CRC-16/MODBUS over *data*."""
    crc = 0x0000
    for byte in data:
        crc = (crc >> 8) ^ _CRC_TABLE[(crc ^ byte) & 0xFF]
    return crc


# ===========================================================================
class LD06LidarNode(Node):
    def __init__(self):
        super().__init__('ld06_lidar_node')

        # --- ROS parameters ---
        self.declare_parameter('port',     '/dev/ttyTHS1')
        self.declare_parameter('baudrate', 230400)
        self.declare_parameter('topic',    'scan')

        self.port     = self.get_parameter('port').value
        self.baudrate = self.get_parameter('baudrate').value
        self.topic    = self.get_parameter('topic').value

        # --- Serial ---
        self.serial = serial.Serial(self.port, self.baudrate, timeout=0.01)

        # --- Publisher ---
        self.publisher = self.create_publisher(LaserScan, self.topic, 10)

        # --- Polling timer (2 ms) ---
        self.timer  = self.create_timer(0.002, self._poll)
        self.buffer = bytearray()

        # --- Debug counters (remove once confirmed working) ---
        self._dbg_headers_seen  = 0   # times we found 0x54 0x2C
        self._dbg_frames_proc   = 0
        self._dbg_bins_filled   = 0
        self._dbg_scans_pub     = 0
        self._dbg_timer = self.create_timer(2.0, self._print_debug)

        # --- Accumulator: fixed-size bins ---
        # Each bin stores (distance_m, intensity).  None = no measurement yet.
        self._bins: list = [None] * NUM_BINS
        self._last_start_angle: float | None = None   # previous frame's start °

        # --- Static TF: base_link → ld06_lidar ---
        self._static_broadcaster = StaticTransformBroadcaster(self)
        self._publish_static_tf()

    # ------------------------------------------------------------------
    # Static transform
    # ------------------------------------------------------------------
    def _publish_static_tf(self):
        now = self.get_clock().now().to_msg()

        # odom → base_link  (gives Foxglove a root frame to anchor everything)
        tf_odom = TransformStamped()
        tf_odom.header.stamp     = now
        tf_odom.header.frame_id  = 'odom'
        tf_odom.child_frame_id   = 'base_link'
        tf_odom.transform.rotation.w = 1.0

        # base_link → ld06_lidar  (sensor offset — identity for now)
        tf_lidar = TransformStamped()
        tf_lidar.header.stamp     = now
        tf_lidar.header.frame_id  = 'base_link'
        tf_lidar.child_frame_id   = 'ld06_lidar'
        tf_lidar.transform.rotation.w = 1.0

        self._static_broadcaster.sendTransform([tf_odom, tf_lidar])

    # ------------------------------------------------------------------
    # Debug log (fires every 2 s)
    # ------------------------------------------------------------------
    def _print_debug(self):
        self.get_logger().warning(
            f"[DBG] headers_seen={self._dbg_headers_seen} "
            f"frames_proc={self._dbg_frames_proc} "
            f"bins_filled={self._dbg_bins_filled} "
            f"scans_pub={self._dbg_scans_pub} "
            f"buf_len={len(self.buffer)}"
        )
        self._dbg_headers_seen  = 0
        self._dbg_frames_proc   = 0
        self._dbg_bins_filled   = 0
        self._dbg_scans_pub     = 0

    # ------------------------------------------------------------------
    # UART polling
    # ------------------------------------------------------------------
    def _poll(self):
        waiting = self.serial.in_waiting
        if waiting:
            self.buffer += self.serial.read(waiting)

        while len(self.buffer) >= FRAME_LEN:
            # Scan for valid header pair
            if self.buffer[0] != HEADER_BYTE_0 or self.buffer[1] != HEADER_BYTE_1:
                self.buffer.pop(0)
                continue

            frame = bytes(self.buffer[:FRAME_LEN])
            self._dbg_headers_seen += 1

            # CRC disabled for now — relying on header sync only
            self._process_frame(frame)
            self._dbg_frames_proc += 1
            self.buffer = self.buffer[FRAME_LEN:]

    # ------------------------------------------------------------------
    # Frame parsing
    # ------------------------------------------------------------------
    def _process_frame(self, pkt: bytes):
        start_angle_deg = (pkt[5] << 8 | pkt[4]) / 100.0   # 0.01 ° resolution
        end_angle_deg   = (pkt[43] << 8 | pkt[42]) / 100.0

        # Detect a full-rotation boundary BEFORE we add this frame's data.
        # A new rotation starts when start_angle wraps back near 0.
        if self._last_start_angle is not None and start_angle_deg < self._last_start_angle:
            # The accumulator holds a complete 360 ° scan — publish it now,
            # then clear it so this frame's points go into the fresh scan.
            self._publish_scan()
            self._bins = [None] * NUM_BINS

        self._last_start_angle = start_angle_deg

        # Normalise end_angle for step calculation across the 360 °/0 ° boundary
        end_for_step = end_angle_deg
        if end_for_step < start_angle_deg:
            end_for_step += 360.0

        step = (end_for_step - start_angle_deg) / (POINTS_PER_FRAME - 1)

        for i in range(POINTS_PER_FRAME):
            offset   = 6 + i * 3
            dist_mm  = pkt[offset] | (pkt[offset + 1] << 8)
            distance = dist_mm / 1000.0          # metres
            intensity = pkt[offset + 2]

            # Filter out-of-range readings
            if not (0.12 <= distance <= 12.0):
                continue

            angle_deg = (start_angle_deg + step * i) % 360.0
            bin_idx   = int(angle_deg / BIN_SIZE_DEG) % NUM_BINS
            self._bins[bin_idx] = (distance, float(intensity))
            self._dbg_bins_filled += 1

    # ------------------------------------------------------------------
    # Publish a full 360 ° LaserScan on /scan
    # ------------------------------------------------------------------
    def _publish_scan(self):
        # Build output arrays.
        # LD06 bin 0 = 0 ° (forward, typically).  ROS LaserScan convention:
        #   index 0 → angle_min = -π   (i.e. 180 °)
        #   index N → angle_max = +π   (i.e. 180 °, wrapped)
        # So we rotate the bin array by half (NUM_BINS // 2 = 360 bins = 180 °).
        half = NUM_BINS // 2
        ranges     = []
        intensities = []

        for i in range(NUM_BINS):
            src = (half - i + NUM_BINS) % NUM_BINS   # mirror the scan
            if self._bins[src] is not None:
                ranges.append(self._bins[src][0])
                intensities.append(self._bins[src][1])
            else:
                # No measurement in this bin → inf = "no return" (ROS convention)
                ranges.append(float('inf'))
                intensities.append(0.0)

        scan = LaserScan()
        scan.header.stamp    = self.get_clock().now().to_msg()
        scan.header.frame_id = 'ld06_lidar'

        scan.angle_min       = ANGLE_MIN           # -π
        scan.angle_max       = ANGLE_MAX           # +π
        scan.angle_increment = ANGLE_INCREMENT     # uniform step

        scan.scan_time       = 0.1                 # LD06 ≈ 10 Hz
        scan.time_increment  = scan.scan_time / NUM_BINS

        scan.range_min = 0.12
        scan.range_max = 12.0

        scan.ranges     = ranges
        scan.intensities = intensities

        self._dbg_scans_pub += 1
        self.publisher.publish(scan)


# ---------------------------------------------------------------------------
def main(args=None):
    rclpy.init(args=args)
    node = LD06LidarNode()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    node.destroy_node()
    rclpy.shutdown()


if __name__ == '__main__':
    main()