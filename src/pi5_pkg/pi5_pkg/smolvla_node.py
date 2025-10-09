# This node will handle the SmolVLA model's inference and publish motor commands.
import rclpy
from rclpy.node import Node
from rclpy.qos import QoSProfile, ReliabilityPolicy, HistoryPolicy, DurabilityPolicy

# Import standard ROS2 messages
from geometry_msgs.msg import Twist
from sensor_msgs.msg import CompressedImage

# Import your custom message
from my_robot_package.msg import HMIButtons

# Import the SmolVLA library (this is a placeholder)
# from smolvla_library import SmolVLA

class VLADecisionNode(Node):
    def __init__(self):
        super().__init__('vla_decision_node')
        self.get_logger().info("VLA Decision Node has started.")

        # Create a QoS profile for reliable communication
        qos_profile = QoSProfile(
            reliability=ReliabilityPolicy.BEST_EFFORT,
            history=HistoryPolicy.KEEP_LAST,
            depth=1
        )
        # Create a publisher for motor commands
        self.cmd_vel_publisher = self.create_publisher(Twist, 'cmd_vel', qos_profile)

        # Create subscribers
        self.image_subscriber = self.create_subscription(
            CompressedImage,
            'camera/compressed_image',
            self.image_callback,
            qos_profile
        )

        self.hmi_subscriber = self.create_subscription(
            HMIButtons,
            'hmi_buttons',
            self.hmi_callback,
            qos_profile
        )

        # Store the latest image and HMI command
        self.latest_image = None
        self.latest_hmi_command = HMIButtons()

        # Initialize SmolVLA model (this is a placeholder)
        # self.smolvla = SmolVLA()
        
    def image_callback(self, msg):
        """Callback for the camera image feed."""
        self.latest_image = msg
        self.process_vla_commands()

    def hmi_callback(self, msg):
        """Callback for the HMI button commands."""
        self.latest_hmi_command = msg
        self.process_vla_commands()
        
    def process_vla_commands(self):
        """
        Main logic to process the image and command with SmolVLA and
        publish the resulting motor commands.
        """
        # Ensure we have a valid image and the control mode is enabled
        if self.latest_image is None or not self.latest_hmi_command.is_smolvla_mode:
            return

        # -----------------------------------------------------------
        # Here is where the SmolVLA inference would happen.
        # It takes the image and a text prompt as input.
        #
        # Example:
        # text_prompt = "Find a tennis ball and move towards it until it fills 20% of the screen."
        # vla_output = self.smolvla.predict(self.latest_image, text_prompt)
        #
        # The 'vla_output' is a vector of numbers, not a string.
        # Let's assume it returns a linear and an angular velocity.
        # For this example, we'll use a placeholder output.
        #
        # -----------------------------------------------------------
        
        # Placeholder for SmolVLA output
        predicted_linear_x = 0.5  # Example value for forward motion
        predicted_angular_z = 0.1 # Example value for a slight turn

        # Create and populate the Twist message
        twist_msg = Twist()
        twist_msg.linear.x = predicted_linear_x
        twist_msg.angular.z = predicted_angular_z
        
        # Publish the command
        self.cmd_vel_publisher.publish(twist_msg)
        self.get_logger().info('Published Twist command: "%s"' % str(twist_msg))


def main(args=None):
    rclpy.init(args=args)
    vla_node = VLADecisionNode()
    rclpy.spin(vla_node)
    vla_node.destroy_node()
    rclpy.shutdown()

if __name__ == '__main__':
    main()
