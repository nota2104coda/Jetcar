#!$HOME/PicoWCar/.venv python3
# Hardware MCU interface node.
#
# I2C + MAVLink schema summary
# ----------------------------
# * Jetson (this node) is I2C master on /dev/i2c-{bus}, polling the MCU slave at
#   address 0x42 (kJetsonI2CAddress in the firmware).
# * Every master read asks for up to 32 bytes. The MCU drains its MAVLink FIFO
#   (fed by sendTelemetry()) and replies with the next contiguous bytes. When
#   the FIFO is empty the MCU returns a single 0x00 byte, so the host must keep
#   reading until entire MAVLink frames are reconstructed.
# * Telemetry messages currently streamed by sendTelemetry():
#     - MAVLINK_MSG_ID_HIGHRES_IMU (linear acceleration, gyro Z, temperature)
#     - MAVLINK_MSG_ID_DISTANCE_SENSOR (IDs 1-4 for front/rear sonar + cliff IR)
#     - MAVLINK_MSG_ID_ESC_TELEMETRY_1_TO_4 (RPM for FR, FL, RR, RL wheels)
# * This node acts as the MAVLink consumer: it parses the byte stream, publishes
#   ROS 2 messages (Imu, Temperature, Range, Odometry) and provides TF odom->base.

import math
from typing import Optional

import rclpy
from rclpy.node import Node
from rclpy.qos import QoSProfile

from geometry_msgs.msg import TransformStamped, Twist
from nav_msgs.msg import Odometry
from sensor_msgs.msg import Imu, Range, Temperature
from tf2_ros import TransformBroadcaster

from smbus2 import SMBus, i2c_msg
from pymavlink.dialects.v20 import ardupilotmega as mavlink2


class HwMcuNode(Node):
    """Bridge MAVLink telemetry from the MCU into ROS 2 topics."""

    def __init__(self) -> None:
        super().__init__('hw_mcu_node')
        self.get_logger().info('Hardware MCU node starting (I2C + MAVLink).')

        # --- Parameters ---
        self.declare_parameter('i2c_bus', 1)
        self.declare_parameter('i2c_address', 0x42)
        self.declare_parameter('i2c_chunk_size', 32)
        self.declare_parameter('i2c_retry_seconds', 2.0)
        self.declare_parameter('poll_period', 1.0)  # 50 Hz polling ideally but start with 1hz
        self.declare_parameter('wheel_base', 0.12)
        self.declare_parameter('wheel_radius', 0.035)
        self.declare_parameter('gear_ratio', 46.0)
        self.declare_parameter('odom_frame_id', 'odom')
        self.declare_parameter('base_frame_id', 'base_link')
        self.declare_parameter('imu_frame_id', 'imu_link')
        self.declare_parameter('command_topic', 'cmd_vel')
        self.declare_parameter('command_mode', 'manual_control')
        self.declare_parameter('command_target_system', 42)
        self.declare_parameter('command_target_component', mavlink2.MAV_COMP_ID_AUTOPILOT1)
        self.declare_parameter('command_source_system', 1)
        self.declare_parameter('command_source_component', mavlink2.MAV_COMP_ID_OBSTACLE_AVOIDANCE)
        self.declare_parameter('manual_linear_max', 1.0)
        self.declare_parameter('manual_yaw_rate_max', 1.0)

        # --- Resolve parameters ---
        self.i2c_bus_num = self.get_parameter('i2c_bus').get_parameter_value().integer_value
        self.i2c_address = self.get_parameter('i2c_address').get_parameter_value().integer_value
        self.i2c_chunk = max(1, self.get_parameter('i2c_chunk_size').get_parameter_value().integer_value)
        self.i2c_retry_seconds = self.get_parameter('i2c_retry_seconds').get_parameter_value().double_value
        self.poll_period = self.get_parameter('poll_period').get_parameter_value().double_value
        self.wheel_base = self.get_parameter('wheel_base').get_parameter_value().double_value
        self.wheel_radius = self.get_parameter('wheel_radius').get_parameter_value().double_value
        self.gear_ratio = self.get_parameter('gear_ratio').get_parameter_value().double_value
        self.odom_frame_id = self.get_parameter('odom_frame_id').get_parameter_value().string_value
        self.base_frame_id = self.get_parameter('base_frame_id').get_parameter_value().string_value
        self.imu_frame_id = self.get_parameter('imu_frame_id').get_parameter_value().string_value
        self.command_topic = self.get_parameter('command_topic').get_parameter_value().string_value
        self.command_mode = self.get_parameter('command_mode').get_parameter_value().string_value.lower()
        self.command_target_system = self.get_parameter('command_target_system').get_parameter_value().integer_value
        self.command_target_component = (
            self.get_parameter('command_target_component').get_parameter_value().integer_value
        )
        self.command_source_system = self.get_parameter('command_source_system').get_parameter_value().integer_value
        self.command_source_component = (
            self.get_parameter('command_source_component').get_parameter_value().integer_value
        )
        self.manual_linear_max = max(
            1e-3, self.get_parameter('manual_linear_max').get_parameter_value().double_value
        )
        self.manual_yaw_rate_max = max(
            1e-3, self.get_parameter('manual_yaw_rate_max').get_parameter_value().double_value
        )

        if self.command_mode not in {'manual_control', 'set_position_target_local_ned'}:
            self.get_logger().warn(
                'command_mode must be either "manual_control" or '
                '"set_position_target_local_ned"; defaulting to manual_control.'
            )
            self.command_mode = 'manual_control'

        # Derived wheel conversion: meters per second per RPM (uses kinematics in firmware)
        rpm_to_rad_per_sec = (2.0 * math.pi) / 60.0
        self.mps_per_rpm = (rpm_to_rad_per_sec * self.wheel_radius) / max(self.gear_ratio, 1e-3)

        # --- State ---
        self.x = 0.0
        self.y = 0.0
        self.theta = 0.0
        self.last_time = self.get_clock().now()
        self.mav_parser = mavlink2.MAVLink(None)
        self.mav_parser.robust_parsing = True
        self.mav_tx = mavlink2.MAVLink(None)
        self.mav_tx.robust_parsing = True
        self.mav_tx.srcSystem = self.command_source_system
        self.mav_tx.srcComponent = self.command_source_component
        self.bus: Optional[SMBus] = None
        self.last_i2c_error_log = 0.0

        # --- ROS interfaces ---
        qos_profile = QoSProfile(depth=10)
        self.tf_broadcaster = TransformBroadcaster(self)
        self.odometry_publisher = self.create_publisher(Odometry, 'odom', qos_profile)
        self.imu_publisher = self.create_publisher(Imu, 'imu/data_raw', qos_profile)
        self.temp_publisher = self.create_publisher(Temperature, 'imu/temperature', qos_profile)
        self.sonar_front_publisher = self.create_publisher(Range, 'sonar/front', qos_profile)
        self.sonar_rear_publisher = self.create_publisher(Range, 'sonar/rear', qos_profile)
        self.cliff_front_publisher = self.create_publisher(Range, 'cliff/front', qos_profile)
        self.cliff_rear_publisher = self.create_publisher(Range, 'cliff/rear', qos_profile)

        self.range_publishers = {
            1: ('sonar/front', self.sonar_front_publisher, Range.ULTRASOUND),
            2: ('sonar/rear', self.sonar_rear_publisher, Range.ULTRASOUND),
            3: ('cliff/front', self.cliff_front_publisher, Range.INFRARED),
            4: ('cliff/rear', self.cliff_rear_publisher, Range.INFRARED),
        }

        self._open_i2c()
        self.timer = self.create_timer(self.poll_period, self.poll_mcu)
        self.command_subscription = self.create_subscription(
            Twist, self.command_topic, self._command_callback, qos_profile
        )

    def _open_i2c(self) -> None:
        """Attempt to open the configured I2C bus."""
        self._close_i2c()
        try:
            self.bus = SMBus(self.i2c_bus_num)
            self.get_logger().info(
                f'Opened /dev/i2c-{self.i2c_bus_num} -> 0x{self.i2c_address:02X} (chunk {self.i2c_chunk}B).'
            )
            self.last_i2c_error_log = 0.0
        except FileNotFoundError:
            self._throttled_i2c_error(
                f'/dev/i2c-{self.i2c_bus_num} not available. Will keep retrying every {self.poll_period}s.'
            )
            self.bus = None
        except OSError as exc:
            self._throttled_i2c_error(f'Failed to open I2C bus: {exc}. Retrying next loop.')
            self.bus = None

    def _close_i2c(self) -> None:
        if self.bus is not None:
            try:
                self.bus.close()
            except OSError:
                pass
        self.bus = None

    def _throttled_i2c_error(self, message: str) -> None:
        now = self.get_clock().now().nanoseconds / 1e9
        if (now - self.last_i2c_error_log) >= max(self.i2c_retry_seconds, 0.5):
            self.get_logger().error(message)
            self.last_i2c_error_log = now

    def poll_mcu(self) -> None:
        if self.bus is None:
            self._open_i2c()
            if self.bus is None:
                return

        try:
            raw_bytes = self._read_chunk()
        except OSError as exc:
            self.get_logger().warn(f'I2C read failed ({exc}). Closing bus; retrying next loop.')
            self._close_i2c()
            return

        for byte in raw_bytes:
            msg = self.mav_parser.parse_char(bytes([byte]))
            if msg is not None:
                self._handle_mavlink_message(msg)

    def _read_chunk(self) -> bytearray:
        assert self.bus is not None
        read_msg = i2c_msg.read(self.i2c_address, self.i2c_chunk)
        self.bus.i2c_rdwr(read_msg)
        return bytearray(read_msg)

    def _handle_mavlink_message(self, message) -> None:
        msg_id = message.get_msgId()
        if msg_id == mavlink2.MAVLINK_MSG_ID_HIGHRES_IMU:
            self._publish_highres_imu(message)
        elif msg_id == mavlink2.MAVLINK_MSG_ID_DISTANCE_SENSOR:
            self._publish_distance_sensor(message)
        elif msg_id == mavlink2.MAVLINK_MSG_ID_ESC_TELEMETRY_1_TO_4:
            self._publish_esc_telemetry(message)

    def _command_callback(self, twist: Twist) -> None:
        if self.bus is None:
            self._open_i2c()
            if self.bus is None:
                return

        if self.command_mode == 'manual_control':
            message = self._build_manual_control_message(twist)
        else:
            message = self._build_velocity_setpoint_message(twist)

        if message is None:
            return

        self._write_mavlink_message(message)

    def _build_manual_control_message(self, twist: Twist):
        scale = 1000.0

        def axis(value: float, maximum: float) -> int:
            normalized = self._clamp(value / maximum, -1.0, 1.0)
            return int(self._clamp(normalized * scale, -scale, scale))

        x = axis(twist.linear.x, self.manual_linear_max)
        y = axis(twist.linear.y, self.manual_linear_max)
        z_norm = self._clamp(twist.linear.z / self.manual_linear_max, -1.0, 1.0)
        z = int(self._clamp((z_norm + 1.0) * 500.0, 0.0, 1000.0))
        r = axis(twist.angular.z, self.manual_yaw_rate_max)
        return self.mav_tx.manual_control_encode(
            self.command_target_system,
            x,
            y,
            z,
            r,
            0,
        )

    def _build_velocity_setpoint_message(self, twist: Twist):
        type_mask = (
            mavlink2.POSITION_TARGET_TYPEMASK_X_IGNORE
            | mavlink2.POSITION_TARGET_TYPEMASK_Y_IGNORE
            | mavlink2.POSITION_TARGET_TYPEMASK_Z_IGNORE
            | mavlink2.POSITION_TARGET_TYPEMASK_AX_IGNORE
            | mavlink2.POSITION_TARGET_TYPEMASK_AY_IGNORE
            | mavlink2.POSITION_TARGET_TYPEMASK_AZ_IGNORE
            | mavlink2.POSITION_TARGET_TYPEMASK_YAW_IGNORE
        )
        return self.mav_tx.set_position_target_local_ned_encode(
            0,
            self.command_target_system,
            self.command_target_component,
            mavlink2.MAV_FRAME_BODY_NED,
            type_mask,
            0.0,
            0.0,
            0.0,
            float(twist.linear.x),
            float(twist.linear.y),
            float(twist.linear.z),
            0.0,
            0.0,
            0.0,
            0.0,
            float(twist.angular.z),
        )

    def _write_mavlink_message(self, message) -> None:
        if self.bus is None:
            self._open_i2c()
            if self.bus is None:
                return

        frame = message.pack(self.mav_tx)
        write_msg = i2c_msg.write(self.i2c_address, frame)
        try:
            self.bus.i2c_rdwr(write_msg)
        except OSError as exc:
            self.get_logger().warn(f'Failed to send MAVLink command: {exc}')
            self._close_i2c()

    @staticmethod
    def _clamp(value: float, minimum: float, maximum: float) -> float:
        return max(minimum, min(maximum, value))

    def _publish_highres_imu(self, msg) -> None:
        stamp = self.get_clock().now().to_msg()
        imu_msg = Imu()
        imu_msg.header.stamp = stamp
        imu_msg.header.frame_id = self.imu_frame_id
        imu_msg.linear_acceleration.x = msg.xacc
        imu_msg.linear_acceleration.y = msg.yacc
        imu_msg.linear_acceleration.z = msg.zacc
        imu_msg.angular_velocity.x = msg.xgyro
        imu_msg.angular_velocity.y = msg.ygyro
        imu_msg.angular_velocity.z = msg.zgyro
        imu_msg.orientation.w = 1.0
        imu_msg.orientation.x = 0.0
        imu_msg.orientation.y = 0.0
        imu_msg.orientation.z = 0.0
        imu_msg.orientation_covariance[0] = -1.0  # Unknown orientation from MCU
        self.imu_publisher.publish(imu_msg)

        temp_msg = Temperature()
        temp_msg.header.stamp = stamp
        temp_msg.header.frame_id = self.imu_frame_id
        temp_msg.temperature = msg.temperature
        temp_msg.variance = 0.0
        self.temp_publisher.publish(temp_msg)

    def _publish_distance_sensor(self, msg) -> None:
        mapping = self.range_publishers.get(msg.id)
        if mapping is None:
            return
        frame_id, publisher, radiation = mapping
        range_msg = Range()
        range_msg.header.stamp = self.get_clock().now().to_msg()
        range_msg.header.frame_id = frame_id
        range_msg.radiation_type = radiation
        range_msg.field_of_view = 0.1
        range_msg.min_range = msg.min_distance / 100.0
        range_msg.max_range = msg.max_distance / 100.0
        range_msg.range = msg.current_distance / 100.0
        publisher.publish(range_msg)

    def _publish_esc_telemetry(self, msg) -> None:
        rpm = [float(value) for value in msg.rpm]
        # sendTelemetry() encodes absolute wheel RPM; until the firmware exports
        # motor direction we only integrate magnitudes for odometry.
        right_linear = self._rpm_to_linear((rpm[0] + rpm[2]) * 0.5)
        left_linear = self._rpm_to_linear((rpm[1] + rpm[3]) * 0.5)
        self.update_odometry(left_linear, right_linear)

    def _rpm_to_linear(self, rpm_value: float) -> float:
        return rpm_value * self.mps_per_rpm

    def update_odometry(self, v_left: float, v_right: float) -> None:
        vx = (v_left + v_right) * 0.5
        wz = (v_right - v_left) / max(self.wheel_base, 1e-3)
        current_time = self.get_clock().now()
        dt = (current_time - self.last_time).nanoseconds / 1e9
        self.last_time = current_time

        delta_x = vx * math.cos(self.theta) * dt
        delta_y = vx * math.sin(self.theta) * dt
        delta_theta = wz * dt

        self.x += delta_x
        self.y += delta_y
        self.theta += delta_theta

        odom_msg = Odometry()
        odom_msg.header.stamp = current_time.to_msg()
        odom_msg.header.frame_id = self.odom_frame_id
        odom_msg.child_frame_id = self.base_frame_id
        odom_msg.pose.pose.position.x = self.x
        odom_msg.pose.pose.position.y = self.y
        odom_msg.pose.pose.orientation = self.euler_to_quaternion(0.0, 0.0, self.theta)
        odom_msg.twist.twist.linear.x = vx
        odom_msg.twist.twist.angular.z = wz
        self.odometry_publisher.publish(odom_msg)

        # Also publish the transform between odom and base_link for TF tree
        t = TransformStamped()
        t.header.stamp = current_time.to_msg()
        t.header.frame_id = self.odom_frame_id
        t.child_frame_id = self.base_frame_id
        t.transform.translation.x = self.x
        t.transform.translation.y = self.y
        t.transform.translation.z = 0.0
        t.transform.rotation = odom_msg.pose.pose.orientation
        self.tf_broadcaster.sendTransform(t)

    def euler_to_quaternion(self, roll: float, pitch: float, yaw: float):
        from geometry_msgs.msg import Quaternion

        cy = math.cos(yaw * 0.5)
        sy = math.sin(yaw * 0.5)
        cp = math.cos(pitch * 0.5)
        sp = math.sin(pitch * 0.5)
        cr = math.cos(roll * 0.5)
        sr = math.sin(roll * 0.5)

        q = Quaternion()
        q.w = cr * cp * cy + sr * sp * sy
        q.x = sr * cp * cy - cr * sp * sy
        q.y = cr * sp * cy + sr * cp * sy
        q.z = cr * cp * sy - sr * sp * cy
        return q


def main(args=None) -> None:
    rclpy.init(args=args)
    node = HwMcuNode()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == '__main__':
    main()
