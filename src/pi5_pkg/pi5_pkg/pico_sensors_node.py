# This node reads processed data from the Pico and publishes it as Odometry.
import rclpy
from rclpy.node import Node
from rclpy.qos import QoSProfile, ReliabilityPolicy, HistoryPolicy, DurabilityPolicy

# Import standard ROS2 messages
from nav_msgs.msg import Odometry
from geometry_msgs.msg import TransformStamped
from tf2_ros import TransformBroadcaster
import math # For converting degrees to radians

class RobotStateNode(Node):
    def __init__(self):
        super().__init__('robot_state_node')
        self.get_logger().info("Robot State Node has started.")

        # Create a QoS profile for reliable communication
        qos_profile = QoSProfile(
            reliability=ReliabilityPolicy.RELIABLE,
            history=HistoryPolicy.KEEP_LAST,
            depth=10
        )

        # Create a publisher for Odometry messages
        self.odometry_publisher = self.create_publisher(Odometry, 'odom', qos_profile)

        # Initialize the TF broadcaster
        self.tf_broadcaster = TransformBroadcaster(self)

        # Use a timer to periodically read from the Pico and publish the state
        # Assume your Pico sends data at 20 Hz
        self.timer = self.create_timer(0.05, self.publish_robot_state)

        # Placeholder for serial connection to Pico
        import serial
        self.pico_serial = serial.Serial('/dev/ttyACM0', 115200)

        # Initial state variables
        self.x = 0.0
        self.y = 0.0
        self.theta = 0.0 # Heading in radians

    def get_data_from_pico(self):
        """
        This function would read the latest speed and heading data from the Pico's serial port.
        """
        # Placeholder data for demonstration
        # In a real-world scenario, you would parse a string from the serial port
        # E.g., "speed=0.5,heading=5.0"
        
        speed = 0.5 # linear speed in m/s
        heading_deg = 5.0 # heading in degrees
        
        # Convert heading to radians
        heading_rad = math.radians(heading_deg)
        
        return speed, heading_rad

    def publish_robot_state(self):
        """
        Read data from the Pico, update robot state, and publish Odometry message.
        """
        # Get the latest data from your Pico
        current_speed, current_heading = self.get_data_from_pico()

        # Update pose estimate (basic integration for demonstration)
        dt = 0.05
        self.x += current_speed * math.cos(self.theta) * dt
        self.y += current_speed * math.sin(self.theta) * dt
        self.theta = current_heading

        # Create and populate the Odometry message
        odom_msg = Odometry()
        odom_msg.header.stamp = self.get_clock().now().to_msg()
        odom_msg.header.frame_id = 'odom' # The global odometry frame
        odom_msg.child_frame_id = 'base_link' # The robot's body frame

        # Set the pose (position and orientation)
        odom_msg.pose.pose.position.x = self.x
        odom_msg.pose.pose.position.y = self.y
        # Convert Euler angle (heading) to Quaternion
        qx = 0.0
        qy = 0.0
        qz = math.sin(self.theta / 2.0)
        qw = math.cos(self.theta / 2.0)
        odom_msg.pose.pose.orientation.x = qx
        odom_msg.pose.pose.orientation.y = qy
        odom_msg.pose.pose.orientation.z = qz
        odom_msg.pose.pose.orientation.w = qw
        
        # Set the twist (velocities)
        odom_msg.twist.twist.linear.x = current_speed
        odom_msg.twist.twist.angular.z = 0.0 # Assuming no angular velocity is received

        # Publish the Odometry message
        self.odometry_publisher.publish(odom_msg)
        self.get_logger().info('Published Odometry message with speed: %s and heading: %s' % (current_speed, current_heading))

        # Also publish the transform between odom and base_link for TF tree
        t = TransformStamped()
        t.header.stamp = self.get_clock().now().to_msg()
        t.header.frame_id = 'odom'
        t.child_frame_id = 'base_link'
        t.transform.translation.x = self.x
        t.transform.translation.y = self.y
        t.transform.rotation.x = qx
        t.transform.rotation.y = qy
        t.transform.rotation.z = qz
        t.transform.rotation.w = qw
        self.tf_broadcaster.sendTransform(t)

def main(args=None):
    rclpy.init(args=args)
    robot_state_node = RobotStateNode()
    rclpy.spin(robot_state_node)
    robot_state_node.destroy_node()
    rclpy.shutdown()

if __name__ == '__main__':
    main()
