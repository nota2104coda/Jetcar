# This node will subscribe to a Twist message and control the robot's motors.
import rclpy
from rclpy.node import Node
from rclpy.qos import QoSProfile, ReliabilityPolicy, HistoryPolicy, DurabilityPolicy

# Import the standard ROS2 Twist message
from geometry_msgs.msg import Twist

class MotorControlNode(Node):
    def __init__(self):
        super().__init__('motor_control_node')
        self.get_logger().info("Motor Control Node has started.")

        # Create a QoS profile for reliable communication
        qos_profile = QoSProfile(
            reliability=ReliabilityPolicy.BEST_EFFORT,
            history=HistoryPolicy.KEEP_LAST,
            depth=1
        )

        # Create a subscriber for the motor commands
        self.subscription = self.create_subscription(
            Twist,
            'cmd_vel',
            self.twist_callback,
            qos_profile
        )
        self.subscription  # prevent unused variable warning

    def twist_callback(self, msg):
        """Callback for the Twist message from the VLA node."""
        self.get_logger().info('Received a Twist command: Linear.x="%s", Angular.z="%s"' % (msg.linear.x, msg.angular.z))

        # -----------------------------------------------------------
        # This is where your actual motor control logic would go.
        # For example, you would convert the linear.x and angular.z
        # values into PWM signals for your Raspberry Pi's motors.
        #
        # Example (pseudo-code):
        # left_motor_speed = (msg.linear.x - msg.angular.z) * motor_scale_factor
        # right_motor_speed = (msg.linear.x + msg.angular.z) * motor_scale_factor
        #
        # set_motor_pwm(left_motor_speed, right_motor_speed)
        #
        # -----------------------------------------------------------
        
def main(args=None):
    rclpy.init(args=args)
    motor_node = MotorControlNode()
    rclpy.spin(motor_node)
    motor_node.destroy_node()
    rclpy.shutdown()

if __name__ == '__main__':
    main()
