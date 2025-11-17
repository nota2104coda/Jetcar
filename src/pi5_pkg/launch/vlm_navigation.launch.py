from launch import LaunchDescription
from launch_ros.actions import Node
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration

def generate_launch_description():
    """Launch SmolVLA node with optional custom prompt."""
    
    # Declare launch arguments
    vlm_prompt_arg = DeclareLaunchArgument(
        'vlm_prompt',
        default_value='Follow the red object. Respond with FORWARD, LEFT, RIGHT, or STOP.',
        description='VLM prompt for navigation task'
    )
    
    return LaunchDescription([
        vlm_prompt_arg,
        
        # SmolVLA decision node (manages VLM container + publishes cmd_vel)
        Node(
            package='pi5_pkg',
            executable='smolvla_node',
            name='smolvla_decision',
            output='screen',
            parameters=[{
                'vlm_prompt': LaunchConfiguration('vlm_prompt'),
            }],
            # Uncomment to enable debug logging
            # arguments=['--ros-args', '--log-level', 'DEBUG'],
        ),
        
        # Your camera node (if on same host)
        # Node(
        #     package='camera_package',
        #     executable='camera_node',
        #     name='camera',
        #     output='screen',
        #     remappings=[
        #         ('image', 'camera/image_raw'),
        #     ],
        # ),
    ])
