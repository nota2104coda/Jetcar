import rclpy
from rclpy.node import Node
from sensor_msgs.msg import Image

class VisionNode(Node):
    def __init__(self):
        super().__init__('vision_node')
        self.publisher_ = self.create_publisher(Image, 'camera/image_raw', 10)
        # TODO: Add lidar, radar fusion, VLA model

    def process_sensors(self):
        # TODO: Read camera, lidar, radar, run VLA model
        pass

def main(args=None):
    rclpy.init(args=args)
    node = VisionNode()
    rclpy.spin(node)
    node.destroy_node()
    rclpy.shutdown()

if __name__ == '__main__':
    main()
