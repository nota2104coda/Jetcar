#!/usr/bin/env python3
import rclpy
from rclpy.node import Node
from std_msgs.msg import String

class camera_node(Node):
    def __init__(self):
        super().__init__("camera_node")
        self.publisher_ = self.create_publisher(String, "camera_topic", 10)
        self.timer = self.create_timer(0.1, self.timer_callback)
        self.get_logger().info("Camera node has been started")

    def timer_callback(self):
        msg = String()
        msg.data = "Hello from camera!"
        self.publisher_.publish(msg)
        self.get_logger().info(f"Publishing: {msg.data}")


def main(args=None):
    rclpy.init(args=args)
    camera = camera_node()
    rclpy.spin(camera)
    
    rclpy.shutdown()

if __name__ == '__main__':
    main()
