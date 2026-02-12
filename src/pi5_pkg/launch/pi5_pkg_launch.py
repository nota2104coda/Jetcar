from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, IncludeLaunchDescription
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration, Command
from launch_ros.actions import Node
from ament_index_python.packages import get_package_share_directory
import os

def generate_launch_description():
    # Get the package directory
    pkg_share_dir = get_package_share_directory('pi5_pkg')
    venv_path = os.path.expanduser('~/PicoWCar/.venv/lib/python3.12/site-packages')
    
    # Declare a launch argument for the robot model file
    robot_model_path_arg = DeclareLaunchArgument(
        'robot_model',
        default_value=os.path.join(pkg_share_dir, 'urdf', 'skid_steer_4wd.urdf'),
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
    hmi_node = Node(
        package='pi5_pkg',
        executable='hmi_node',
        name='hmi_node',
        output='screen',
        env={
                'PYTHONPATH': f"{venv_path}:{os.environ.get('PYTHONPATH', '')}"
            }
    )
    hw_mcu_node = Node(
        package='pi5_pkg',
        executable='hw_mcu_node',
        name='hw_mcu_node',
        output='screen',
        env={
                'PYTHONPATH': f"{venv_path}:{os.environ.get('PYTHONPATH', '')}"
            }
    )

    realsense_node = IncludeLaunchDescription(
        PythonLaunchDescriptionSource([
            os.path.join(pkg_share_dir, 'launch', 'realsense_optimized.launch.py')
        ])
    )
    
    adaptive_resolution_node = Node(
        package='pi5_pkg',
        executable='adaptive_resolution_node',
        name='adaptive_resolution_node',
        output='screen'
    )

    ld06_lidar_node = Node(
        package='pi5_pkg',
        executable='ld06_lidar_node',
        name='ld06_lidar_node',
        output='screen'
    )

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
        hmi_node,
        hw_mcu_node,
        realsense_node,
        adaptive_resolution_node,
        ld06_lidar_node
    ])
