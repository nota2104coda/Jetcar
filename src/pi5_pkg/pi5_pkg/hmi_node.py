#!$HOME/PicoWCar/.venv/bin/python

# I want a simple locally hosted webpage on the raspberry pi 5. it will have a text box for entering commands. 
# it will have a button for "enter command". it will have a toggle for 'command vs buttons'. 
# it will have forward and backward button and clockwise and anticlockwise button. 
# It will, finally, have a power on/off button. Make a ROS2 node python script for this

import rclpy
from rclpy.node import Node
from std_msgs.msg import String
from pywebio.platform.flask import webio_view
from pywebio import start_server
from pywebio.output import put_scope, put_buttons, put_text, put_input, put_row, use_scope
from pywebio.session import set_env, run_js
from pywebio.pin import pin, put_checkbox
import threading
import time

class WebserverNode(Node):
    """
    A ROS2 node that hosts a web server for controlling a robot.
    """
    def __init__(self):
        super().__init__("webserver_node")
        self.publisher_ = self.create_publisher(String, "robot_commands", 10)
        self.get_logger().info("Webserver node has been started and is publishing to 'robot_commands'")

    def publish_command(self, command: str):
        """Publishes a command to the 'robot_commands' topic."""
        msg = String()
        msg.data = command
        self.publisher_.publish(msg)
        self.get_logger().info(f'Publishing: "{msg.data}"')

def pywebio_app(node: WebserverNode):
    """
    The main application logic for the PyWebIO web interface.
    """
    set_env(title="PicoWCar Controller")

    put_scope('main_scope')

    def send_command(command: str):
        """Callback to publish a command."""
        node.publish_command(command)
        with use_scope('log', clear=False):
             put_text(f"{time.strftime('%H:%M:%S')}: Sent command '{command}'")
        run_js('$("#log").scrollTop($("#log")[0].scrollHeight)')

    def command_input_handler(command_text: str):
        """Handles text input commands."""
        if command_text:
            send_command(command_text)

    def toggle_view(command_mode):
        """Switches between command input and button controls."""
        if command_mode:
            with use_scope('control_view', clear=True):
                put_row([
                    put_input('command_text', placeholder='Enter command...'),
                    put_buttons([{'label': 'Enter Command', 'value': 'enter'}],
                                onclick=lambda _: command_input_handler(pin.command_text))
                ])
        else:
            with use_scope('control_view', clear=True):
                put_buttons([
                    {'label': '⬆️ Forward', 'value': 'forward'},
                    {'label': '⬇️ Backward', 'value': 'backward'},
                    {'label': '🔄 Clockwise', 'value': 'clockwise'},
                    {'label': '🔄 Anticlockwise', 'value': 'anticlockwise'},
                ], onclick=send_command, group=True)

    with use_scope('main_scope'):
        put_row([
            put_checkbox('mode_toggle', options=[{'label': 'Command Mode', 'value': 'cmd', 'selected': False}],
                         onchange=lambda val: toggle_view('cmd' in val)),
            None, # Spacer
            put_buttons([{'label': 'POWER ON', 'value': 'power_on', 'color': 'success'},
                         {'label': 'POWER OFF', 'value': 'power_off', 'color': 'danger'}],
                        onclick=send_command)
        ], size='1fr 1fr 1fr')
        put_scope('control_view')
        put_scope('log').style('max-height: 200px; overflow-y: scroll; border: 1px solid #ccc; padding: 5px;')

    # Initial view
    toggle_view(False)

def ros_thread(node):
    """Function to run the ROS2 node."""
    rclpy.spin(node)
    node.destroy_node()
    rclpy.shutdown()

def main(args=None):
    rclpy.init(args=args)
    webserver_node = WebserverNode()

    # Run rclpy.spin in a separate thread
    ros_spin_thread = threading.Thread(target=ros_thread, args=(webserver_node,))
    ros_spin_thread.daemon = True
    ros_spin_thread.start()

    # Start the PyWebIO server
    # Use 0.0.0.0 to make it accessible on your local network
    start_server(lambda: pywebio_app(webserver_node), port=8080, host='0.0.0.0', debug=False)

if __name__ == '__main__':
    main()