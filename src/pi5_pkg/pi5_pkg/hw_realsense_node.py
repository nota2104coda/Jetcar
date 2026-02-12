import rclpy
from rclpy.node import Node
from rclpy.qos import qos_profile_sensor_data
from sensor_msgs.msg import Image, CameraInfo
from cv_bridge import CvBridge
import pyrealsense2 as rs
import cv2
import numpy as np

class RealsenseNode(Node):
    def __init__(self):
        super().__init__('hw_realsense_node')
        
        # Publishers
        self.rgb_pub = self.create_publisher(Image, '/camera_raw', qos_profile_sensor_data)
        self.depth_pub = self.create_publisher(Image, '/depth_raw', qos_profile_sensor_data)
        self.camera_info_pub = self.create_publisher(CameraInfo, '/camera_info', qos_profile_sensor_data)
        
        # RealSense pipeline setup
        self.pipeline = rs.pipeline()
        self.config = rs.config()
        self.config.enable_stream(rs.stream.color, 640, 480, rs.format.bgr8, 15)
        self.config.enable_stream(rs.stream.depth, 640, 480, rs.format.z16, 15)
        
        self.profile = self.pipeline.start(self.config)
        self.intrinsics = self.profile.get_stream(rs.stream.color).as_video_stream_profile().get_intrinsics()
        
        # Alignment object
        self.align = rs.align(rs.stream.color)
        self.bridge = CvBridge()
        
        # Timer for periodic frame capture
        self.timer = self.create_timer(1.0/15.0, self.capture_frames)
        self.get_logger().info('RealSense D435 node started')
    
    def capture_frames(self):
        frames = self.pipeline.wait_for_frames()
        aligned_frames = self.align.process(frames)
        
        color_frame = aligned_frames.get_color_frame()
        depth_frame = aligned_frames.get_depth_frame()
        
        if not color_frame or not depth_frame:
            return
        
        # Convert to numpy arrays
        color_image = np.asanyarray(color_frame.get_data())
        depth_image = np.asanyarray(depth_frame.get_data())
        
        # Create and publish ROS2 messages
        timestamp = self.get_clock().now().to_msg()
        
        # RGB image
        rgb_msg = self.bridge.cv2_to_imgmsg(color_image, encoding="bgr8")
        rgb_msg.header.stamp = timestamp
        rgb_msg.header.frame_id = "camera_optical_frame"
        self.rgb_pub.publish(rgb_msg)
        
        # Depth image
        # Using 16UC1 (millimeter) encoding to save bandwidth (2 bytes vs 4 bytes per pixel)
        # Visualization tools (Foxglove/RViz) handle 16UC1 as mm automatically.
        depth_msg = self.bridge.cv2_to_imgmsg(depth_image, encoding="mono16")
        depth_msg.header.stamp = timestamp
        depth_msg.header.frame_id = "camera_optical_frame"
        self.depth_pub.publish(depth_msg)
        
        # Camera info
        camera_info = self.create_camera_info()
        camera_info.header.stamp = timestamp
        camera_info.header.frame_id = "camera_optical_frame"
        self.camera_info_pub.publish(camera_info)
    
    def create_camera_info(self):
        camera_info = CameraInfo()
        camera_info.height = self.intrinsics.height
        camera_info.width = self.intrinsics.width
        camera_info.distortion_model = "plumb_bob"
        # Ensure all values are floats to prevent AssertionError
        camera_info.d = [float(c) for c in self.intrinsics.coeffs]
        camera_info.k = [float(self.intrinsics.fx), 0.0, float(self.intrinsics.ppx),
                         0.0, float(self.intrinsics.fy), float(self.intrinsics.ppy),
                         0.0, 0.0, 1.0]
        camera_info.p = [float(self.intrinsics.fx), 0.0, float(self.intrinsics.ppx), 0.0,
                         0.0, float(self.intrinsics.fy), float(self.intrinsics.ppy), 0.0,
                         0.0, 0.0, 1.0, 0.0]
        return camera_info
    
    def destroy_node(self):
        self.pipeline.stop()
        super().destroy_node()

def main(args=None):
    rclpy.init(args=args)
    node = RealsenseNode()
    rclpy.spin(node)
    node.destroy_node()
    rclpy.shutdown()

if __name__ == '__main__':
    main()