from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration, Command
from launch_ros.actions import Node
from ament_index_python.packages import get_package_share_directory
import os

def generate_launch_description():
    # Get the package directory
    pkg_share_dir = get_package_share_directory('pi5_pkg')
    
    # Declare a launch argument for the robot model file
    robot_model_path_arg = DeclareLaunchArgument(
        'robot_model',
        default_value=os.path.join(pkg_share_dir, 'urdf', 'robot_model.urdf.xacro'),
        description='Path to the robot URDF/xacro file'
    )
    
    # Load the robot description from the URDF file
    robot_description = Command(['xacro ', LaunchConfiguration('robot_model')])

    # Node to publish the robot's state
    robot_state_publisher_node = Node(
        package='robot_state_publisher',
        executable='robot_state_publisher',
        name='robot_state_publisher',
        output='screen',
        parameters=[{'robot_description': robot_description}]
    )

    # Your custom Python nodes
    camera_node = Node(
        package='pi5_pkg',
        executable='camera_node.py',
        name='camera',
        output='screen'
    )
    # # smolvla_node = Node(
    #     package='pi5_pkg',
    #     executable='smolvla_node.py',
    #     name='vla_decision_node',
    #     output='screen'
    # )
    
    # motor_control_node = Node(
    #     package='pi5_pkg',
    #     executable='motor_control_node.py',
    #     name='motor_control_node',
    #     output='screen'
    # )
    
    # robot_state_node = Node(
    #     package='pi5_pkg',
    #     executable='robot_state_node.py',
    #     name='robot_state_node',
    #     output='screen'
    # )

    # Launch description with all the nodes
    return LaunchDescription([
        robot_model_path_arg,
        robot_state_publisher_node,
        vla_node,
        motor_control_node,
        robot_state_node
    ])
