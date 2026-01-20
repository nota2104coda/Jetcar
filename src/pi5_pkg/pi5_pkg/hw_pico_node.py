#!$HOME/PicoWCar/.venv python3
# This node reads processed data from the Pico and publishes it as Odometry.
import rclpy
from rclpy.node import Node
from rclpy.qos import QoSProfile

# Import standard ROS2 messages
from nav_msgs.msg import Odometry
from geometry_msgs.msg import TransformStamped
from tf2_ros import TransformBroadcaster

from sensor_msgs.msg import Range # For Sonar, Cliff sensors
# from sensor_msgs.msg import LaserScan # For LD2450
# from sensor_msgs.msg import PointCloud2 # For VL53L5X
#g it check

import math
import serial
import json
import time

class PicoSensorsNode(Node):
    def __init__(self):
        super().__init__('pico_sensors_node')
        self.get_logger().info("Pico Sensors Node has started.")

        # --- Parameters ---
        # Use UART device (not USB serial). On Jetson Orin Nano, this is usually /dev/ttyTHS1 or /dev/ttyTHS2
        # See Jetson hardware docs for correct UART port. Default below is typical for Jetson Orin Nano UART1.
        self.declare_parameter('serial_port', '/dev/ttyTHS1')
        self.declare_parameter('baud_rate', 115200)
        self.declare_parameter('wheel_base', 0.15) # meters, distance between left and right wheels
        self.declare_parameter('wheel_radius', 0.03) # meters
        self.declare_parameter('odom_frame_id', 'odom')
        self.declare_parameter('base_frame_id', 'base_link')

        # --- Get Parameters ---
        # Store parameters as attributes for cleaner access later
        self.serial_port = self.get_parameter('serial_port').get_parameter_value().string_value
        self.baud_rate = self.get_parameter('baud_rate').get_parameter_value().integer_value
        self.wheel_base = self.get_parameter('wheel_base').get_parameter_value().double_value
        self.wheel_radius = self.get_parameter('wheel_radius').get_parameter_value().double_value
        self.odom_frame_id = self.get_parameter('odom_frame_id').get_parameter_value().string_value
        self.base_frame_id = self.get_parameter('base_frame_id').get_parameter_value().string_value

        # --- QoS Profile ---
        qos_profile = QoSProfile(depth=10)

        # --- Publishers ---
        self.odometry_publisher = self.create_publisher(Odometry, 'odom', qos_profile)
        self.rear_sonar_publisher = self.create_publisher(Range, 'sonar/rear', qos_profile)
        self.cliff_front_publisher = self.create_publisher(Range, 'cliff/front', qos_profile)
        self.cliff_rear_publisher = self.create_publisher(Range, 'cliff/rear', qos_profile)
        # Add publishers for LD2450 and VL53L5X as needed

        # --- TF Broadcaster ---
        self.tf_broadcaster = TransformBroadcaster(self)

        # --- UART Connection (Jetson Orin Nano) ---
        try:
            self.pico_serial = serial.Serial(self.serial_port, self.baud_rate, timeout=1)
            self.get_logger().info(f"Successfully connected to Pico via UART on {self.serial_port} (Jetson Orin Nano)")
            # Send handshake message to Pico
            try:
                self.pico_serial.write(b"CMD,stop\n")
                self.get_logger().info("Sent handshake: CMD,stop")
            except Exception as e:
                self.get_logger().warn(f"Failed to send handshake: {e}")
            # Wait for Pico to send a valid line (handshake)
            self.get_logger().info("Waiting for Pico to send UART message...")
            handshake_timeout = 5.0  # seconds
            start_time = time.time()
            got_pico = False
            while time.time() - start_time < handshake_timeout:
                if self.pico_serial.in_waiting > 0:
                    line = self.pico_serial.readline().decode('utf-8').strip()
                    if line:
                        self.get_logger().info(f"Received handshake from Pico: {line}")
                        got_pico = True
                        break
                time.sleep(0.05)
            if not got_pico:
                self.get_logger().error(f"No UART message from Pico after {handshake_timeout} seconds. Declaring Pico dead, will keep retrying.")
                self.pico_alive = False
            else:
                self.pico_alive = True
        except serial.SerialException as e:
            self.get_logger().error(f"Failed to connect to Pico on {self.serial_port}: {e}")
            rclpy.shutdown()
            return
    def read_and_publish(self):
        """
        Read a line from UART, parse it according to UART schema, and publish data to respective topics.
        If Pico is not alive, keep trying to reconnect.
        """
        if hasattr(self, 'pico_alive') and not self.pico_alive:
            # Try to reconnect/handshake
            if self.pico_serial.in_waiting > 0:
                line = self.pico_serial.readline().decode('utf-8').strip()
                if line:
                    self.get_logger().info(f"Pico reconnected: {line}")
                    self.pico_alive = True
            return

        # --- State Variables ---
        self.x = 0.0
        self.y = 0.0
        self.theta = 0.0 # Heading in radians
        self.last_time = self.get_clock().now()

        # --- Timer ---
        # The timer will attempt to read and process data. The actual rate
        # will depend on how fast the Pico sends data.
        self.timer = self.create_timer(0.02, self.read_and_publish) # 50 Hz loop
        # Example: send a command to Pico (uncomment to use)
        # self.send_command("forward")

    def read_and_publish(self):
        """
        Read a line from UART, parse it according to UART schema, and publish data to respective topics.
        """
        if not self.pico_serial.in_waiting > 0:
            return

        try:
            line = self.pico_serial.readline().decode('utf-8').strip()
            if not line:
                return
            if line.startswith("TEL,"):
                # Parse telemetry line
                fields = line.split(',')
                if len(fields) != 17:
                    self.get_logger().warn(f"Malformed TEL line: {line}")
                    return
                # Unpack fields
                ts_ms = int(fields[1])
                ax, ay, az = float(fields[2]), float(fields[3]), float(fields[4])
                gx, gy, gz = float(fields[5]), float(fields[6]), float(fields[7])
                temp_c = float(fields[8])
                sonar_f_cm = int(fields[9])
                sonar_r_cm = int(fields[10])
                cliff_f = bool(fields[11])
                cliff_r = bool(fields[12])
                spdFL = float(fields[13])
                spdFR = float(fields[14])
                spdRL = float(fields[15])
                spdRR = float(fields[16])

                # Publish odometry using wheel speeds (FL, FR, RL, RR)
                left_rad_s = (spdFL + spdRL) / 2.0
                right_rad_s = (spdFR + spdRR) / 2.0
                self.update_odometry(left_rad_s, right_rad_s)

                # Publish sonar (front and rear)
                self.publish_range('sonar/rear', self.rear_sonar_publisher, sonar_r_cm / 100.0)
                # Optionally publish front sonar if needed
                # self.publish_range('sonar/front', self.front_sonar_publisher, sonar_f_cm / 100.0)

                # Publish cliff sensors
                self.publish_range('cliff/front', self.cliff_front_publisher, 0.2 if cliff_f == 0 else 0.0)
                self.publish_range('cliff/rear', self.cliff_rear_publisher, 0.2 if cliff_r == 0 else 0.0)
                # You can add more publishers for IMU, temp, etc. as needed
            elif line.startswith("CMD,"):
                # Ignore incoming CMD lines (should not happen, but for completeness)
                pass
            else:
                # Ignore lines with unexpected prefix
                return
        except Exception as e:
            self.get_logger().error(f"An error occurred: {e}")
        except serial.SerialException:
            self.get_logger().error("Serial connection lost. Attempting to reconnect...")
            while not self.try_reconnect():
                time.sleep(1)
    def send_command(self, verb_or_set, *args):
        """
        Send a command to Pico as per UART schema.
        Usage:
            send_command("forward")
            send_command("set", tqFR, tqFL, tqRR, tqRL)
        """
        if verb_or_set == "set" and len(args) == 4:
            cmd = f"CMD,set,{args[0]},{args[1]},{args[2]},{args[3]}\n"
        else:
            cmd = f"CMD,{verb_or_set}\n"
        try:
            self.pico_serial.write(cmd.encode('utf-8'))
            self.get_logger().info(f"Sent command: {cmd.strip()}")
        except Exception as e:
            self.get_logger().error(f"Failed to send command: {e}")

    def try_reconnect(self):
        serial_port = self.get_parameter('serial_port').get_parameter_value().string_value
        baud_rate = self.get_parameter('baud_rate').get_parameter_value().integer_value
        try:
            self.pico_serial = serial.Serial(serial_port, baud_rate, timeout=1)
            self.get_logger().info(f"Reconnected to Pico on {serial_port}")
            return True
        except serial.SerialException:
            self.get_logger().warn("Reconnection failed, will retry...")
            return False

    def update_odometry(self, left_rad_s, right_rad_s):
        # Calculate linear and angular velocities
        v_left = left_rad_s * self.wheel_radius
        v_right = right_rad_s * self.wheel_radius

        vx = (v_right + v_left) / 2.0
        wz = (v_right - v_left) / self.wheel_base

        # Integrate odometry
        # Note: self.last_time was initialized in __init__
        current_time = self.get_clock().now()
        dt = (current_time - self.last_time).nanoseconds / 1e9
        self.last_time = current_time

        delta_x = vx * math.cos(self.theta) * dt
        delta_y = vx * math.sin(self.theta) * dt
        delta_theta = wz * dt

        self.x += delta_x
        self.y += delta_y
        self.theta += delta_theta

        # --- Publish Odometry Message ---
        odom_msg = Odometry()
        odom_msg.header.stamp = current_time.to_msg()
        odom_msg.header.frame_id = self.odom_frame_id
        odom_msg.child_frame_id = self.base_frame_id

        # Set the pose (position and orientation)
        odom_msg.pose.pose.position.x = self.x
        odom_msg.pose.pose.position.y = self.y
        odom_msg.pose.pose.orientation = self.euler_to_quaternion(0, 0, self.theta)

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
        t.transform.rotation = odom_msg.pose.pose.orientation
        self.tf_broadcaster.sendTransform(t)

    def publish_range(self, frame_id, publisher, distance):
        msg = Range()
        msg.header.stamp = self.get_clock().now().to_msg()
        msg.header.frame_id = frame_id
        msg.radiation_type = Range.ULTRASOUND if 'sonar' in frame_id else Range.INFRARED
        msg.field_of_view = 0.1 # Radians, example value
        msg.min_range = 0.02 # meters
        msg.max_range = 4.0 if 'sonar' in frame_id else 0.5 # meters to make it drive cautiously if frame missing
        msg.range = float(distance)
        publisher.publish(msg)

    def euler_to_quaternion(self, roll, pitch, yaw):
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

def main(args=None):
    rclpy.init(args=args)
    node = PicoSensorsNode()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()

if __name__ == '__main__':
    main()
