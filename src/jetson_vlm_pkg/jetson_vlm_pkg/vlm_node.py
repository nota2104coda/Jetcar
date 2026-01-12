import rclpy
from rclpy.node import Node
from sensor_msgs.msg import Image
from geometry_msgs.msg import Twist
from cv_bridge import CvBridge
import numpy as np
from vlm_service import VLMInferenceService
import threading

class VLMNavigationNode(Node):
    def __init__(self):
        super().__init__('vlm_navigation_node')
        
        self.bridge = CvBridge()
        self.vlm_service = None
        self.latest_image = None
        self.lock = threading.Lock()
        
        # Subscribers
        self.image_sub = self.create_subscription(
            Image,
            'image_raw',
            self.image_callback,
            10
        )
        
        # Publishers
        self.cmd_vel_pub = self.create_publisher(Twist, 'cmd_vel', 10)
        
        # Services
        self.create_service(
            GetVLMCommand,
            'get_vlm_command',
            self.vlm_callback
        )
        
        # Initialize VLM (lazy load on first request)
        self.vlm_initialized = False
        
        self.get_logger().info('VLM Navigation Node initialized')
    
    def image_callback(self, msg):
        with self.lock:
            self.latest_image = self.bridge.imgmsg_to_cv2(msg)
    
    def vlm_callback(self, request, response):
        if not self.vlm_initialized:
            self.get_logger().info('Initializing VLM on first request...')
            self.vlm_service = VLMInferenceService()
            self.vlm_initialized = True
        
        if self.latest_image is not None and self.vlm_service:
            prompt = request.prompt
            command = self.vlm_service.infer(self.latest_image, prompt)
            response.command = command
        
        return response

def main(args=None):
    rclpy.init(args=args)
    node = VLMNavigationNode()
    rclpy.spin(node)
    node.destroy_node()
    rclpy.shutdown()

if __name__ == '__main__':
    main()