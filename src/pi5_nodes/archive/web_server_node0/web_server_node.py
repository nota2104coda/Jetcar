import rclpy
from rclpy.node import Node
from std_msgs.msg import String
from flask import Flask, request, render_template
import threading

app = Flask(__name__)

@app.route('/')
def index():
    return "Rover Web Interface"

@app.route('/command', methods=['POST'])
def command():
    cmd = request.form.get('cmd')
    # TODO: Publish command to ROS2
    return f"Command received: {cmd}"

class WebServerNode(Node):
    def __init__(self):
        super().__init__('web_server_node')
        self.publisher_ = self.create_publisher(String, 'web_command', 10)
        threading.Thread(target=self.run_flask, daemon=True).start()

    def run_flask(self):
        app.run(host='0.0.0.0', port=8080)

def main(args=None):
    rclpy.init(args=args)
    node = WebServerNode()
    rclpy.spin(node)
    node.destroy_node()
    rclpy.shutdown()

if __name__ == '__main__':
    main()
