#!$HOME/PicoWCar/.venv python3
# Hardware MCU interface node.
#
# USB serial + MAVLink schema summary
# -----------------------------------
# * Jetson (this node) connects to the MCU over USB CDC (e.g., /dev/ttyUSB0)
#   and continuously drains its MAVLink byte stream.
# * The MCU keeps sending MAVLink frames back-to-back; pymavlink resynchronizes
#   even if reads split frames across chunk boundaries.
# * Telemetry messages currently streamed by sendTelemetry():
#     - MAVLINK_MSG_ID_HIGHRES_IMU (linear acceleration, gyro Z, temperature)
#     - MAVLINK_MSG_ID_DISTANCE_SENSOR (IDs 1-4 for front/rear sonar + cliff IR)
#     - MAVLINK_MSG_ID_ESC_TELEMETRY_1_TO_4 (RPM for FR, FL, RR, RL wheels)
# * This node acts as the MAVLink consumer: it parses the byte stream, publishes
#   ROS 2 messages (Imu, Temperature, Range, Odometry) and provides TF odom->base.

import math
import random
from typing import Optional

import rclpy
from rclpy.node import Node
from rclpy.qos import QoSProfile

from geometry_msgs.msg import TransformStamped, Twist
from nav_msgs.msg import Odometry
from sensor_msgs.msg import Imu, Range, Temperature
from tf2_ros import TransformBroadcaster

import serial
from serial import SerialException
from pymavlink.dialects.v20 import ardupilotmega as mavlink2



class HwMcuNode(Node):
    """Bridge MAVLink telemetry from the MCU into ROS 2 topics."""

    def __init__(self) -> None:
        super().__init__('hw_mcu_node')
        self.get_logger().set_level(rclpy.logging.LoggingSeverity.DEBUG)
        self.get_logger().info('Hardware MCU node starting (USB serial + MAVLink).')
        self._mavlink_buffer = bytearray()

        # --- Parameters ---
        self.declare_parameter('serial_port', '/dev/ttyUSB0')
        self.declare_parameter('serial_baud_rate', 921600)
        self.declare_parameter('serial_timeout', 0.01)
        self.declare_parameter('serial_chunk_size', 256)
        self.declare_parameter('serial_retry_seconds', 2.0)
        self.declare_parameter('poll_period', 0.01)
        self.declare_parameter('wheel_base', 0.12)
        self.declare_parameter('wheel_radius', 0.035)
        self.declare_parameter('gear_ratio', 46.0)
        self.declare_parameter('odom_frame_id', 'odom')
        self.declare_parameter('base_frame_id', 'base_link')
        self.declare_parameter('imu_frame_id', 'imu_link')
        self.declare_parameter('command_topic', 'cmd_vel')
        self.declare_parameter('command_mode', 'set_actuator_control_target')
        self.declare_parameter('command_target_system', 42)
        self.declare_parameter('command_target_component', mavlink2.MAV_COMP_ID_AUTOPILOT1)
        self.declare_parameter('command_source_system', 200)
        self.declare_parameter('command_source_component', 191)
        self.declare_parameter('manual_linear_max', 1.0)
        self.declare_parameter('manual_yaw_rate_max', 1.0)

        # --- Resolve parameters ---
        self.serial_port = (
            self.get_parameter('serial_port').get_parameter_value().string_value or '/dev/ttyUSB0'
        )
        self.serial_baud_rate = self.get_parameter('serial_baud_rate').get_parameter_value().integer_value
        self.serial_timeout = max(
            0.0, self.get_parameter('serial_timeout').get_parameter_value().double_value
        )
        self.serial_chunk = max(
            1, self.get_parameter('serial_chunk_size').get_parameter_value().integer_value
        )
        self.serial_retry_seconds = (
            self.get_parameter('serial_retry_seconds').get_parameter_value().double_value
        )
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

        if self.command_mode not in {'set_actuator_control_target', 'set_position_target_local_ned'}:
            self.get_logger().warn(
                'command_mode must be either "set_actuator_control_target" or '
                '"set_position_target_local_ned"; defaulting to set_actuator_control_target.'
            )
            self.command_mode = 'set_actuator_control_target'

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
        self.serial: Optional[serial.Serial] = None
        self.last_serial_error_log = 0.0

        # --- ROS interfaces ---
        qos_profile = QoSProfile(depth=10)
        self.tf_broadcaster = TransformBroadcaster(self)

        from std_msgs.msg import Float32MultiArray
        self.odometry_publisher = self.create_publisher(Odometry, '/odom', qos_profile)
        self.imu_publisher = self.create_publisher(Imu, '/mcu/imu', qos_profile)
        self.esc_publisher = self.create_publisher(Float32MultiArray, '/esc_telemetry', qos_profile)
        self.range_front_publisher = self.create_publisher(Range, '/mcu/range/front', qos_profile)
        self.range_rear_publisher = self.create_publisher(Range, '/mcu/range/rear', qos_profile)
        self.cliff_front_publisher = self.create_publisher(Range, '/mcu/cliff/front', qos_profile)
        self.cliff_rear_publisher = self.create_publisher(Range, '/mcu/cliff/rear', qos_profile)

        self.range_publishers = {
            1: ('/mcu/range/front', self.range_front_publisher, Range.ULTRASOUND),
            2: ('/mcu/range/rear', self.range_rear_publisher, Range.ULTRASOUND),
            3: ('/mcu/cliff/front', self.cliff_front_publisher, Range.INFRARED),
            4: ('/mcu/cliff/rear', self.cliff_rear_publisher, Range.INFRARED),
        }

        self._open_serial()
        self.timer = self.create_timer(self.poll_period, self.poll_mcu)
        self.command_subscription = self.create_subscription(
            Twist, self.command_topic, self._command_callback, qos_profile
        )

    def _open_serial(self) -> None:
        """Open the configured MCU serial port if possible."""
        self._close_serial()
        try:
            self.serial = serial.Serial(
                self.serial_port,
                baudrate=self.serial_baud_rate,
                timeout=self.serial_timeout,
            )
            self.serial.reset_input_buffer()
            self.serial.reset_output_buffer()
            self.get_logger().info(
                f'Opened {self.serial_port} @ {self.serial_baud_rate} baud '
                f'(chunk {self.serial_chunk}B).'
            )
            self.last_serial_error_log = 0.0
        except (SerialException, OSError) as exc:
            self._throttled_serial_error(
                f'Failed to open serial port {self.serial_port}: {exc}. Retrying next loop.'
            )
            self.serial = None

    def _close_serial(self) -> None:
        if self.serial is not None:
            try:
                self.serial.close()
            except SerialException:
                pass
        self.serial = None

    def _throttled_serial_error(self, message: str) -> None:
        now = self.get_clock().now().nanoseconds / 1e9
        if (now - self.last_serial_error_log) >= max(self.serial_retry_seconds, 0.5):
            self.get_logger().error(message)
            self.last_serial_error_log = now

    def poll_mcu(self) -> None:
        if self.serial is None:
            self._open_serial()
            if self.serial is None:
                return
        # Publish actuator_control_message on UART after reading serial data
        actuator_msg = self._build_actuator_control_message(Twist())
        if actuator_msg is not None:
            self._write_mavlink_message(actuator_msg)

        self.get_logger().debug('Polling MCU via serial...')
        try:
            raw_bytes = self._read_serial_chunk()
        except SerialException as exc:
            self.get_logger().warn(f'Serial read failed ({exc}). Closing port; retrying next loop.')
            self._close_serial()
            return

        if not raw_bytes:
            return

        self.get_logger().debug(
            f'Serial read returned {len(raw_bytes)} bytes: {[f"{b:02X}" for b in raw_bytes]}'
        )
        self._mavlink_buffer.extend(raw_bytes)
        # Parse as many messages as possible from the buffer
        i = 0
        while i < len(self._mavlink_buffer):
            msg = self.mav_parser.parse_char(bytes([self._mavlink_buffer[i]]))
            if msg is not None:
                # self.get_logger().info(f'Parsed MAVLink message: {msg.get_type()} (ID {msg.get_msgId()})')
                self._handle_mavlink_message(msg)
                # Remove bytes up to and including this message from buffer
                # pymavlink does not expose consumed length, so we conservatively clear up to i
                self._mavlink_buffer = self._mavlink_buffer[i+1:]
                i = 0
            else:
                i += 1

    def _read_serial_chunk(self) -> bytearray:
        assert self.serial is not None
        available = self.serial.in_waiting if hasattr(self.serial, 'in_waiting') else 0
        to_read = available if available > 0 else self.serial_chunk
        data = self.serial.read(to_read)
        return bytearray(data)

    def _handle_mavlink_message(self, message) -> None:
        msg_id = message.get_msgId()
        if msg_id == mavlink2.MAVLINK_MSG_ID_HIGHRES_IMU:
            self._publish_highres_imu(message)
        elif msg_id == mavlink2.MAVLINK_MSG_ID_DISTANCE_SENSOR:
            self._publish_distance_sensor(message)
        elif msg_id == mavlink2.MAVLINK_MSG_ID_ESC_TELEMETRY_1_TO_4:
            self._publish_esc_telemetry(message)

    def _command_callback(self, twist: Twist) -> None:
        if self.serial is None:
            self._open_serial()
            if self.serial is None:
                return

        if self.command_mode == 'set_actuator_control_target':
            message = self._build_actuator_control_message(twist)
        else:
            message = self._build_velocity_setpoint_message(twist)

        if message is None:
            return

        self._write_mavlink_message(message)

    def _build_actuator_control_message(self, twist: Twist):
        # Map Twist to actuator controls (4 wheels: FR, RR, FL, RL)
        # For a diff-drive, map linear.x to both, angular.z to left/right diff
        # Here, we assume 4 actuators, values in [-1, 1]
        actuators = [0.0] * 8
        # Simple diff-drive mapping for 4 wheels
        v = max(min(twist.linear.x, 1.0), -1.0)
        w = max(min(twist.angular.z, 1.0), -1.0)
        left = v - w
        right = v + w
        # actuators[0] = right  # FR
        # actuators[1] = right   # RR
        # actuators[2] = left  # FL
        # actuators[3] = left   # RL
        # actuators[0] = 0.3 + 0.1*random.random()  # FR
        # actuators[1] = -0.3 + 0.1*random.random()  # RR
        # actuators[2] = 0.5 + 0.1*random.random()  # FL
        # actuators[3] = -0.5 + 0.1*random.random()  # RL
        actuators[0] = 0.3  # FR
        actuators[1] = 0.3   # RR
        actuators[2] = -0.3  # FL
        actuators[3] = -0.3   # RL
        # Remaining actuators (4-7) left at 0.0
        # pymavlink expects 6 arguments: time_boot_ms, target_system, target_component, group_mlx, controls, flags
        # pymavlink expects: time_usec, group_mlx, target_system, target_component, controls
        self.get_logger().debug('sending data to UART serial...')
        return self.mav_tx.set_actuator_control_target_encode(
            0,  # time_usec
            0,  # group_mlx (0 = default)
            self.command_target_system,
            self.command_target_component,
            actuators
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
        if self.serial is None:
            self._open_serial()
            if self.serial is None:
                return

        frame = message.pack(self.mav_tx)
        try:
            self.serial.write(frame)
            self.serial.flush()
        except SerialException as exc:
            self.get_logger().warn(f'Failed to send MAVLink command: {exc}')
            self._close_serial()

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
        # Publish as Float32MultiArray for /esc_telemetry
        from std_msgs.msg import Float32MultiArray
        esc_msg = Float32MultiArray()
        esc_msg.data = rpm
        self.esc_publisher.publish(esc_msg)
        # sendTelemetry() encodes absolute wheel RPM; until the firmware exports
        # motor direction we only integrate magnitudes for odometry.
        right_linear = self._rpm_to_linear((rpm[0] + rpm[1]) * 0.5)
        left_linear = self._rpm_to_linear((rpm[2] + rpm[3]) * 0.5)
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
