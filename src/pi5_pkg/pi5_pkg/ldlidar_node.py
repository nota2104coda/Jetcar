import rclpy
from rclpy.node import Node
from sensor_msgs.msg import LaserScan
import serial
import math
from geometry_msgs.msg import TransformStamped
from tf2_ros import StaticTransformBroadcaster

FRAME_LEN = 47
HEADER = 0x54
POINTS_PER_FRAME = 12
DEG2RAD = math.pi / 180.0


class LD06LidarNode(Node):
    def __init__(self):
        super().__init__('ld06_lidar_node')

        self.declare_parameter('port', '/dev/ttyTHS1')
        self.declare_parameter('baudrate', 230400)
        self.declare_parameter('topic', 'scan')

        self.port = self.get_parameter('port').value
        self.baudrate = self.get_parameter('baudrate').value
        self.topic = self.get_parameter('topic').value

        self.serial = serial.Serial(self.port, self.baudrate, timeout=0.01)
        self.publisher = self.create_publisher(LaserScan, self.topic, 10)

        self.timer = self.create_timer(0.002, self.poll)
        self.buffer = bytearray()

        # Accumulator for full scan
        self.angle_ranges = {}  # angle(rad) -> distance(m)
        self.angle_intensities = {}
        self.last_angle = None

        # Static TF
        self.static_broadcaster = StaticTransformBroadcaster(self)
        self.publish_static_tf()

    # ---------------- TF ----------------
    def publish_static_tf(self):
        tf = TransformStamped()
        tf.header.stamp = self.get_clock().now().to_msg()
        tf.header.frame_id = 'base_link'
        tf.child_frame_id = 'ld06_lidar'
        tf.transform.rotation.w = 1.0
        self.static_broadcaster.sendTransform(tf)

    # ---------------- UART ----------------
    def poll(self):
        if self.serial.in_waiting:
            self.buffer += self.serial.read(self.serial.in_waiting)

        while len(self.buffer) >= FRAME_LEN:
            if self.buffer[0] != HEADER or self.buffer[1] != 0x2C:
                self.buffer.pop(0)
                continue

            frame = self.buffer[:FRAME_LEN]
            if not self.check_crc(frame):
                self.buffer.pop(0)
                continue

            self.process_frame(frame)
            self.buffer = self.buffer[FRAME_LEN:]

    # ---------------- CRC ----------------
    def check_crc(self, packet):
        crc = 0
        for b in packet[:-1]:
            crc ^= b
        return crc == packet[-1]

    # ---------------- Frame parsing ----------------
    def process_frame(self, packet):
        start_angle = (packet[5] << 8 | packet[4]) / 100.0
        end_angle = (packet[43] << 8 | packet[42]) / 100.0

        # Normalize wraparound
        if end_angle < start_angle:
            end_angle += 360.0

        step = (end_angle - start_angle) / (POINTS_PER_FRAME - 1)

        for i in range(POINTS_PER_FRAME):
            offset = 6 + i * 3
            distance = (packet[offset + 1] << 8 | packet[offset]) / 1000.0
            intensity = packet[offset + 2]
            angle = (start_angle + step * i) % 360.0
            angle_rad = angle * DEG2RAD

            if 0.12 <= distance <= 12.0:
                self.angle_ranges[angle_rad] = distance
                self.angle_intensities[angle_rad] = intensity

        # Detect full rotation
        if self.last_angle is not None and start_angle < self.last_angle:
            self.publish_scan()
            self.angle_ranges.clear()
            self.angle_intensities.clear()

        self.last_angle = start_angle

    # ---------------- Publish LaserScan ----------------
    def publish_scan(self):
        if not self.angle_ranges:
            return

        angles = sorted(self.angle_ranges.keys())
        ranges = [self.angle_ranges[a] for a in angles]
        intensities = [float(self.angle_intensities[a]) for a in angles]

        scan = LaserScan()
        scan.header.stamp = self.get_clock().now().to_msg()
        scan.header.frame_id = 'ld06_lidar'

        scan.angle_min = angles[0]
        scan.angle_max = angles[-1]
        scan.angle_increment = (scan.angle_max - scan.angle_min) / max(len(ranges) - 1, 1)

        scan.scan_time = 0.1          # ~10 Hz LD06
        scan.time_increment = scan.scan_time / len(ranges)

        scan.range_min = 0.12
        scan.range_max = 12.0
        scan.ranges = ranges
        scan.intensities = intensities

        self.publisher.publish(scan)


def main(args=None):
    rclpy.init(args=args)
    node = LD06LidarNode()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    node.destroy_node()
    rclpy.shutdown()
