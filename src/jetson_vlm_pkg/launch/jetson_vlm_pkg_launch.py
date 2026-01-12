from launch import LaunchDescription
from launch_ros.actions import Node

def generate_launch_description():
    return LaunchDescription([
        Node(
            package='jetson_vlm_pkg',
            executable='vlm_node',
            name='vlm_navigation',
            output='screen',
            parameters=[{
                'model': 'liuhaotian/llava-v1.5-7b',
                'quantization': 'q4f16_ft',
            }]
        ),
        # Your camera node
        Node(
            package='camera_bridge_pkg',
            executable='camera_node',
            name='camera',
            output='screen'
        ),
    ])