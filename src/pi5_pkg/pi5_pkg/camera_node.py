#!$HOME/PicoWCar/.venv python3

import rclpy
from rclpy.qos import QoSProfile, ReliabilityPolicy
# ... and use ReliabilityPolicy.BEST_EFFORT instead of rclpy.qos.ReliabilityPolicy.BEST_EFFORT
from rclpy.node import Node
from sensor_msgs.msg import Image
import cv2
import numpy as np
import time
from cv_bridge import CvBridge

CAMERA_INDEX = 0

class CameraNode(Node):
    def __init__(self):
        super().__init__("camera_node")
        self.publisher_ = self.create_publisher(Image, "camera/image_raw", QoSProfile(depth=1, reliability=ReliabilityPolicy.BEST_EFFORT))
        self.bridge = CvBridge()
        self.cap = cv2.VideoCapture(CAMERA_INDEX, cv2.CAP_V4L2)
        if not self.cap.isOpened():
            self.get_logger().error("Could not open video device at /dev/video0.")
            raise RuntimeError("Camera not found")
        # Set resolution (optional)
        self.cap.set(cv2.CAP_PROP_FRAME_WIDTH, 640)
        self.cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 480)
        time.sleep(2)  # Camera warm-up
        self.timer = self.create_timer(0.1, self.timer_callback)
        self.get_logger().info("Camera node has been started and camera opened.")

    def timer_callback(self):
        ret, frame = self.cap.read()
        if not ret:
            self.get_logger().error("Failed to capture frame from camera.")
            return
        # Convert OpenCV image (BGR) to ROS Image message
        msg = self.bridge.cv2_to_imgmsg(frame, encoding="bgr8")
        self.publisher_.publish(msg)
        self.get_logger().info(f"Published camera frame at {self.get_clock().now().to_msg()}")

    def destroy_node(self):
        if hasattr(self, 'cap'):
            self.cap.release()
        super().destroy_node()



def main(args=None):
    rclpy.init(args=args)
    camera = CameraNode()
    try:
        rclpy.spin(camera)
    finally:
        camera.destroy_node()
        rclpy.shutdown()

if __name__ == '__main__':
    main()
