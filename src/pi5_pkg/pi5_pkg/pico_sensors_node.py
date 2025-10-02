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

import math
import serial
import json
import time

class PicoSensorsNode(Node):
    def __init__(self):
        super().__init__('pico_sensors_node')
        self.get_logger().info("Pico Sensors Node has started.")

        # --- Parameters ---
        self.declare_parameter('serial_port', '/dev/ttyACM0')
        self.declare_parameter('baud_rate', 115200)
        self.declare_parameter('wheel_base', 0.15) # meters, distance between left and right wheels
        self.declare_parameter('wheel_radius', 0.035) # meters
        self.declare_parameter('odom_frame_id', 'odom')
        self.declare_parameter('base_frame_id', 'base_link')

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

        # --- Serial Connection ---
        serial_port = self.get_parameter('serial_port').get_parameter_value().string_value
        baud_rate = self.get_parameter('baud_rate').get_parameter_value().integer_value
        try:
            self.pico_serial = serial.Serial(serial_port, baud_rate, timeout=1)
            self.get_logger().info(f"Successfully connected to Pico on {serial_port}")
        except serial.SerialException as e:
            self.get_logger().error(f"Failed to connect to Pico on {serial_port}: {e}")
            rclpy.shutdown()
            return

        # --- State Variables ---
        self.x = 0.0
        self.y = 0.0
        self.theta = 0.0 # Heading in radians

        # --- Timer ---
        # The timer will attempt to read and process data. The actual rate
        # will depend on how fast the Pico sends data.
        self.timer = self.create_timer(0.02, self.read_and_publish) # 50 Hz loop

    def read_and_publish(self):
        """
        Read a line from serial, parse it, and publish data to respective topics.
        """
        if not self.pico_serial.in_waiting > 0:
            return

        try:
            line = self.pico_serial.readline().decode('utf-8').strip()
            if not line:
                return
            
            data = json.loads(line)
            
            # Process and publish each piece of data
            if 'wl' in data:
                # Assuming data['wl'] = [front_left, front_right, rear_left, rear_right]
                # For a differential drive, we average front and rear wheels
                left_rad_s = (data['wl'][0] + data['wl'][2]) / 2.0
                right_rad_s = (data['wl'][1] + data['wl'][3]) / 2.0
                self.update_odometry(left_rad_s, right_rad_s)

            if 'sonar' in data:
                self.publish_range('sonar/rear', self.rear_sonar_publisher, data['sonar'])

            if 'cliff' in data:
                # Assuming 0=clear, 1=cliff. We can publish a max_range for clear.
                self.publish_range('cliff/front', self.cliff_front_publisher, 0.5 if data['cliff'][0] else 0.0)
                self.publish_range('cliff/rear', self.cliff_rear_publisher, 0.5 if data['cliff'][1] else 0.0)

            # Add handlers for 'tof' and 'lidar' data here

        except json.JSONDecodeError:
            self.get_logger().warn(f"Received malformed JSON: {line}")
        except Exception as e:
            self.get_logger().error(f"An error occurred: {e}")
        except serial.SerialException:
            self.get_logger().error("Serial connection lost. Attempting to reconnect...")
            while not self.try_reconnect():
                time.sleep(1)

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
        wheel_radius = self.get_parameter('wheel_radius').get_parameter_value().double_value
        wheel_base = self.get_parameter('wheel_base').get_parameter_value().double_value

        # Calculate linear and angular velocities
        v_left = left_rad_s * wheel_radius
        v_right = right_rad_s * wheel_radius

        vx = (v_right + v_left) / 2.0
        wz = (v_right - v_left) / wheel_base

        # Integrate odometry
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
        odom_msg.header.frame_id = self.get_parameter('odom_frame_id').get_parameter_value().string_value
        odom_msg.child_frame_id = self.get_parameter('base_frame_id').get_parameter_value().string_value

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
        t.header.frame_id = self.get_parameter('odom_frame_id').get_parameter_value().string_value
        t.child_frame_id = self.get_parameter('base_frame_id').get_parameter_value().string_value
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
        msg.max_range = 4.0 if 'sonar' in frame_id else 0.5 # meters
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
