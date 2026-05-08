import os
from launch import LaunchDescription
from launch.actions import IncludeLaunchDescription, DeclareLaunchArgument
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import Command, LaunchConfiguration
from launch_ros.actions import Node
from ament_index_python.packages import get_package_share_directory

def generate_launch_description():
    pkg_real = get_package_share_directory('jetcar_real')
    pkg_description = get_package_share_directory('jetcar_description')
    
    # 0. Robot State Publisher (URDF via Xacro)
    xacro_path = os.path.join(pkg_description, 'urdf', 'jetcar.urdf.xacro')
    robot_description_content = Command(['xacro ', xacro_path, ' sim_mode:=false'])
    
    robot_state_publisher_node = Node(
        package='robot_state_publisher',
        executable='robot_state_publisher',
        name='robot_state_publisher',
        output='screen',
        parameters=[{'robot_description': robot_description_content}]
    )

    # 1. Lidar
    ld06_launch = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(os.path.join(pkg_real, 'launch', 'ld06.launch.py'))
    )

    # 2. Camera (RealSense)
    realsense_launch = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(os.path.join(pkg_real, 'launch', 'realsense_optimized.launch.py'))
    )

    # 3. MCU Node (Hardware version)
    hw_mcu_node = Node(
        package='jetcar_real',
        executable='hw_mcu_node',
        name='hw_mcu_node',
        parameters=[{
            'serial_port': '/dev/serial/by-id/usb-Silicon_Labs_CP2104_USB_to_UART_Bridge_Controller_02CZJZRS-if00-port0', 
            'serial_baud_rate': 1000000,
            'enable_tf_broadcast': False,
            'poll_period': 0.005,
            'control_period': 0.02,
            'command_topic_manual': 'cmd_vel_manual',
            'command_topic_nav': 'cmd_vel_nav',
        }]
    )

    return LaunchDescription([
        robot_state_publisher_node,
        ld06_launch,
        realsense_launch,
        hw_mcu_node,
    ])
