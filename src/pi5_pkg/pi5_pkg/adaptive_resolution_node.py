import numbers

from numpy import size
import rclpy
from rclpy.node import Node
from nav_msgs.msg import Odometry
from rcl_interfaces.msg import Parameter, ParameterType, ParameterValue
from rcl_interfaces.srv import SetParameters
import math

class AdaptiveResolutionNode(Node):
    def __init__(self):
        super().__init__('adaptive_resolution_node')
        
        # Parameters
        self.declare_parameter('low_speed_threshold', 0.2)
        self.declare_parameter('high_speed_threshold', 0.3)
        self.declare_parameter('enable_dynamic_decimation', True)
        
        self.low_speed_limit = self.get_parameter('low_speed_threshold').value
        self.high_speed_limit = self.get_parameter('high_speed_threshold').value
        
        # State
        self.current_state = 'HIGH_RES' # HIGH_RES (mag 1) or LOW_RES (mag 2/3)
        
        # Camera Node Name (hardcoded to default official node name, adjust if needed)
        self.camera_node_name = '/camera/camera'
        
        # Subscriber
        self.odom_sub = self.create_subscription(
            Odometry,
            '/odom',
            self.odom_callback,
            10
        )
        
        # Service Client for Parameter Setting
        # 1. Setup (The "Phone Call")
        # The node prepares a "phone line" (Service Client) to talk to the RealSense camera node.
        # Specifically, it wants to talk to the /camera/camera/set_parameters service, which is the standard ROS 2 way to change settings of another node remotely.

        self.param_client = self.create_client(SetParameters, f'{self.camera_node_name}/set_parameters')
        
        self.get_logger().info('Adaptive Resolution Node Started')
        self.get_logger().info(f'Waiting for {self.camera_node_name}/set_parameters service...')
        self.param_client.wait_for_service(timeout_sec=5.0)
        
    def odom_callback(self, msg):
        if not self.get_parameter('enable_dynamic_decimation').value:
            return

        #1. Calculate linear speed
        # it listens to /odom and calcualtes linear speed as sqrt(vx^2 + vy^2). If the speed exceeds the high threshold, it calls the set_decimation function to switch to low resolution (decimation 2). If the speed drops below the low threshold, it switches back to high resolution (decimation 1). The hysteresis prevents rapid toggling between resolutions when the speed is around the threshold.
        vx = msg.twist.twist.linear.x
        vy = msg.twist.twist.linear.y
        speed = math.sqrt(vx**2 + vy**2)
        
        #3. Hysteresis Logic to prevent flickering
        # IF you are driving FAST (> 0.3 m/s), it switches to "Low Res".
        # IF you slow down BELOW 0.2 m/s, it switches back to "High Res".
        # Why two numbers (0.3 and 0.2)? This is called Hysteresis. If we just used one number (e.g., 0.25), and you drove at exactly 0.25 m/s, the camera would glitch out switching between High/Low resolution 10 times a second. This gap prevents that.
        
        if self.current_state == 'HIGH_RES' and speed > self.high_speed_limit:
            self.set_decimation(2) # Low Resolution (Half size)
            self.current_state = 'LOW_RES'
            self.get_logger().info(f'Speed {speed:.2f} > {self.high_speed_limit}: Switching to Low Res (Decimation 2)')
            
        elif self.current_state == 'LOW_RES' and speed < self.low_speed_limit:
            self.set_decimation(1) # High Resolution (Original size)
            self.current_state = 'HIGH_RES'
            self.get_logger().info(f'Speed {speed:.2f} < {self.low_speed_limit}: Switching to High Res (Decimation 1)')

    def set_decimation(self, magnitude):
        if not self.param_client.service_is_ready():
            return
            
        req = SetParameters.Request()
        val = ParameterValue(type=ParameterType.PARAMETER_INTEGER, integer_value=magnitude)
        param = Parameter(name='decimation_filter.filter_magnitude', value=val)
        req.parameters = [param]
        # 4. Changing the Setting 
        # This is where it actually "makes the call" to the RealSense node.
        # It tells the camera: "Hey, set your decimation_filter.filter_magnitude to X".
        # Magnitude 1: Keep original size (424x240).
        # Magnitude 2: Divide size by 2 (212x120) -> 4x less data!
        future = self.param_client.call_async(req)
        # We don't block waiting for result to keep odom callback fast

def main(args=None):
    rclpy.init(args=args)
    node = AdaptiveResolutionNode()
    rclpy.spin(node)
    node.destroy_node()
    rclpy.shutdown()

if __name__ == '__main__':
    main()
