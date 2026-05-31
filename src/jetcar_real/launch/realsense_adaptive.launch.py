import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import IncludeLaunchDescription, DeclareLaunchArgument
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node

def generate_launch_description():
    pkg_realsense = get_package_share_directory('realsense2_camera')
    
    # Arguments
    serial_no = LaunchConfiguration('serial_no')
    # Use 'camera' as default name
    camera_name = LaunchConfiguration('camera_name')

    # Realsense Launch
    rs_launch = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(pkg_realsense, 'launch', 'rs_launch.py')
        ),
        launch_arguments={
            'serial_no': serial_no,
            'camera_name': camera_name,
            'enable_color': 'true',
            'enable_depth': 'true',
            'enable_infra1': 'false',
            'enable_infra2': 'false',
            # Set resolution and FPS (width,height,fps)
            'rgb_camera.color_profile': '640,480,15',
            'depth_module.depth_profile': '640,480,15',
            'frames_queue_size': '2', # Reduce queue size to minimize latency
            # Enable the decimation filter so we can modify it
            'filters': 'decimation',
        }.items()
    )

    # Adaptive Resolution Node
    adaptive_node = Node(
        package='jetcar_common',
        executable='adaptive_resolution_node',
        name='adaptive_resolution',
        output='screen',
        parameters=[{
            # Construct camera node name: e.g. /camera/camera
            'camera_node_name': ['/', camera_name, '/', camera_name],
            'low_speed_threshold': 0.2,
            'high_speed_threshold': 0.3,
            'enable_dynamic_decimation': True
        }]
    )

    return LaunchDescription([
        DeclareLaunchArgument('serial_no', default_value='', description='Serial number'),
        DeclareLaunchArgument('camera_name', default_value='camera', description='Camera name'),
        rs_launch,
        adaptive_node
    ])
