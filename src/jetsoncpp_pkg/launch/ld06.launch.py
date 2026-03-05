import os
from launch import LaunchDescription
from launch.actions import IncludeLaunchDescription, DeclareLaunchArgument
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration, PathJoinSubstitution
from launch_ros.substitutions import FindPackageShare
from launch_ros.actions import Node

def generate_launch_description():
    ldlidar_pkg = FindPackageShare('ldlidar_ros2')
    
    # Arguments
    portname = '/dev/serial/by-id/usb-Silicon_Labs_CP2104_USB_to_UART_Bridge_Controller_024TKTOY-if00-port0'
    port = LaunchConfiguration('port', default=portname)
    lidar_frame = LaunchConfiguration('lidar_frame', default='ld06_lidar')
    topic_name = LaunchConfiguration('topic_name', default='scan')

    # Include the official launch file
    # Note: The official launch file might hardcode some parameters or frame names, 
    # so often it's better to verify its content.
    # But for integration, we'll include it or just run the node directly if simpler.
    # Let's run the node directly to ensure we control the parameters (especially port).
    
    ld06_node = Node(
        package='ldlidar_ros2',
        executable='ldlidar_ros2_node',
        name='ld06_lidar',
        output='screen',
        parameters=[
            {'product_name': 'LDLiDAR_LD06'},
            {'laser_scan_topic_name': topic_name},
            {'point_cloud_2d_topic_name': 'pointcloud2d'},
            {'frame_id': lidar_frame},
            {'port_name': port},
            {'serial_baudrate': 230400},
            {'laser_scan_dir': True},
            {'enable_angle_crop_func': False},
            {'angle_crop_min': 135.0},
            {'angle_crop_max': 225.0},
            {'range_min': 0.02},
            {'range_max': 12.0}
        ],
        respawn=True,
        respawn_delay=2.0,
    )
    
    # PWM Control Node for Motor (GPIO12)
    lidar_pwm_node = Node(
        package='jetsoncpp_pkg',
        executable='lidar_pwm.py',
        name='lidar_pwm_control',
        output='screen'
    )

    return LaunchDescription([
        DeclareLaunchArgument('port', default_value=portname, description='Serial port for LIDAR'),
        DeclareLaunchArgument('lidar_frame', default_value='ld06_lidar', description='Frame ID for LIDAR'),
        DeclareLaunchArgument('topic_name', default_value='scan', description='Topic name for laser scan'),
        lidar_pwm_node,
        ld06_node
    ])

