import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, IncludeLaunchDescription
from launch.substitutions import LaunchConfiguration, Command, PathJoinSubstitution
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch_ros.actions import Node
from launch_ros.substitutions import FindPackageShare

def generate_launch_description():
    pkg_jetson_cpp = get_package_share_directory('jetson_cpp_pkg')
    
    # 1. Robot Model (URDF)
    # Check if urdf exists in share directory
    urdf_path = os.path.join(pkg_jetson_cpp, 'urdf', 'skid_steer_4wd.urdf')
    
    # Use xacro if installed, else raw urdf
    # Assuming xacro is installed and necessary for this urdf
    robot_description_content = Command(['xacro ', urdf_path])
    
    robot_state_publisher_node = Node(
        package='robot_state_publisher',
        executable='robot_state_publisher',
        name='robot_state_publisher',
        output='screen',
        parameters=[{'robot_description': robot_description_content}]
    )

    # 2. HMI Node (C++)
    hmi_node = Node(
        package='jetson_cpp_pkg',
        executable='hmi_node',
        name='hmi_node',
        output='screen',
        parameters=[{'use_sim_time': False}]
    )

    # 3. Hardware MCU Node (C++)
    # Handles serial communication with MCU via MAVLink
    hw_mcu_node = Node(
        package='jetson_cpp_pkg',
        executable='hw_mcu_node',
        name='hw_mcu_node',
        output='screen',
        parameters=[{
            'serial_port': '/dev/ttyUSB0', 
            'serial_baud_rate': 921600,
            'poll_period': 0.01,
            'control_period': 0.05
        }]
    )

    # 4. Adaptive Resolution Node (C++)
    adaptive_resolution_node = Node(
        package='jetson_cpp_pkg',
        executable='adaptive_resolution_node',
        name='adaptive_resolution_node',
        output='screen'
    )

    # 5. Realsense Camera (Optimized)
    # Using the local launch file copied from pi5_pkg
    realsense_launch = IncludeLaunchDescription(
        PythonLaunchDescriptionSource([
            os.path.join(pkg_jetson_cpp, 'launch', 'realsense_optimized.launch.py')
        ])
    )

    # 6. Lidar (LD06)
    # Using the local launch file
    ld06_launch = IncludeLaunchDescription(
        PythonLaunchDescriptionSource([
            os.path.join(pkg_jetson_cpp, 'launch', 'ld06.launch.py')
        ]),
        launch_arguments={'port': '/dev/ttyUSB1', 'lidar_frame': 'ld06_lidar'}.items()
    )

    return LaunchDescription([
        robot_state_publisher_node,
        hmi_node,
        hw_mcu_node,
        adaptive_resolution_node,
        realsense_launch,
        ld06_launch
    ])
