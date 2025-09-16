import rclpy
from rclpy.node import Node
import serial

class ControlNode(Node):
    def __init__(self):
        super().__init__('control_node')
        # TODO: Setup serial connection to Pico W
        self.serial_port = None

    def send_command(self, command):
        # TODO: Send command to Pico W
        pass

    def read_sensors(self):
        # TODO: Read sensor data from Pico W
        pass

def main(args=None):
    rclpy.init(args=args)
    node = ControlNode()
    rclpy.spin(node)
    node.destroy_node()
    rclpy.shutdown()

if __name__ == '__main__':
    main()
