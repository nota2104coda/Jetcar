#!$HOME/PicoWCar/.venv/bin/python

# I want a simple locally hosted webpage on the raspberry pi 5. it will have a text box for entering commands. 
# it will have a button for "enter command". it will have a toggle for 'command vs buttons'. 
# it will have forward and backward button and clockwise and anticlockwise button. 
# It will, finally, have a power on/off button. Make a ROS2 node python script for this

import rclpy
from rclpy.node import Node
from std_msgs.msg import Bool
import threading
import time
import subprocess
from robot_msgs.msg import ButtonStates  # Update with your actual message import
from geometry_msgs.msg import Twist

class HMINode(Node):
    """
    ROS2 node for aggregating button states and publishing them for hw_pico_node.
    """
    def __init__(self):
        super().__init__('hmi_node')
        # Start Foxglove bridge
        self.foxglove_process = subprocess.Popen(["ros2", "launch", "foxglove_bridge", "foxglove_bridge_launch.xml"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        self.get_logger().info("Foxglove bridge launched.")
        self.button_states = ButtonStates()
        self.create_subscription(Twist, '/cmd_vel_manual', self.twist_callback, 10)
        self.create_subscription(Bool, '/stop_button', self.stop_callback, 10)
        self.create_subscription(Bool, '/auto_mode_button', self.auto_mode_callback, 10)
        self.publisher_ = self.create_publisher(ButtonStates, '/hmi/button_states', 10)
        self.timer = self.create_timer(0.05, self.publish_states)
        self.last_twist_time = self.get_clock().now()
        # Parameterized timeout duration in seconds
        self.cmd_vel_timeout = 5.0
        self.get_logger().info("hmi_node started, publishing button states.")

    def twist_callback(self, msg):
        self.last_twist_time = self.get_clock().now()
        self.button_states.forward = msg.linear.x > 0.1
        self.button_states.backward = msg.linear.x < -0.1
        self.button_states.left_turn = msg.angular.z > 0.1
        self.button_states.right_turn = msg.angular.z < -0.1
        self.button_states.stop_button = abs(msg.linear.x) < 0.1 and abs(msg.linear.y) < 0.1 and abs(msg.angular.z) < 0.1

    def stop_callback(self, msg):
        self.button_states.stop_button = msg.data

    def auto_mode_callback(self, msg):
        self.button_states.auto_mode_button = msg.data

    def publish_states(self):
        # Check if no twist message received recently, assume released
        time_since_last = self.get_clock().now() - self.last_twist_time
        # if (time_since_last.nanoseconds / 1e9) > self.cmd_vel_timeout:
            # self.button_states.forward = False
            # self.button_states.backward = False
            # self.button_states.left_turn = False
            # self.button_states.right_turn = False
            # self.button_states.stop_button = True
        self.publisher_.publish(self.button_states)


def main(args=None):
    rclpy.init(args=args)
    node = HMINode()
    rclpy.spin(node)
    node.destroy_node()
    # Terminate Foxglove bridge if still running
    if hasattr(node, 'foxglove_process') and node.foxglove_process.poll() is None:
        node.foxglove_process.terminate()
        node.foxglove_process.wait()
    rclpy.shutdown()

if __name__ == '__main__':
    main()