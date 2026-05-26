#!/usr/bin/env python3
import rclpy
from rclpy.node import Node
from geometry_msgs.msg import PoseStamped

class GoalFixer(Node):
    def __init__(self):
        super().__init__('goal_fixer')
        self.subscription = self.create_subscription(
            PoseStamped,
            '/move_base_simple/goal',
            self.listener_callback,
            10)
        self.publisher = self.create_publisher(PoseStamped, '/goal_pose', 10)
        self.get_logger().info('Goal Fixer Node started. Relaying /move_base_simple/goal to /goal_pose with timestamp fixing.')

    def listener_callback(self, msg):
        # Fix timestamp if it is 0
        if msg.header.stamp.sec == 0 and msg.header.stamp.nanosec == 0:
            msg.header.stamp = self.get_clock().now().to_msg()
            self.get_logger().info('Fixed 0 timestamp on goal message.')
        
        # Ensure frame_id is valid (default to map if empty)
        if not msg.header.frame_id:
            msg.header.frame_id = 'map'
            
        self.publisher.publish(msg)

def main(args=None):
    rclpy.init(args=args)
    goal_fixer = GoalFixer()
    rclpy.spin(goal_fixer)
    goal_fixer.destroy_node()
    rclpy.shutdown()

if __name__ == '__main__':
    main()
