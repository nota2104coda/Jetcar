import os
from launch import LaunchDescription
from launch.actions import IncludeLaunchDescription, DeclareLaunchArgument
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration
from ament_index_python.packages import get_package_share_directory

def generate_launch_description():
    # Find the realsense2_camera package
    realsense_dir = get_package_share_directory('realsense2_camera')
    
    # Define the optimized launch arguments
    # 1. Low FPS (15) & Resolution (424x240) to reduce bandwidth
    # 2. 'SensorData' QoS (Best Effort) to prevent WiFi lag spikes
    # 3. Decimation Filter to reduce depth processing load
    # note GEmini installed sudo apt install -y ros-humble-compressed-image-transport
    # and apt list --installed | grep compressed-image-transport
    #superoptimised for latency in streaming and low cpu usage, not for high quality depth or pointclouds
    # launch_args = {
    #     'rgb_camera.color_profile': '424x240x15',
    #     'depth_module.depth_profile': '424x240x15',
    #     'rgb_camera.color_qos': 'SENSOR_DATA',
    #     'depth_module.depth_qos': 'SENSOR_DATA',
    #     'align_depth.enable': 'true',
    #     'enable_sync': 'true',
    #     'pointcloud.enable': 'false', 
    #     'decimation_filter.enable': 'true', # Reduces depth resolution by 2x (very fast)
    #     'spatial_filter.enable': 'true',    # Smooths depth
    #     'temporal_filter.enable': 'true',   # Stabilizes depth
    # }

    #alternative, more pixels per image but higher latency and CPU usage:
    launch_args = {
        'rgb_camera.color_profile': '640x480x15',
        'depth_module.depth_profile': '640x480x15',
        'rgb_camera.color_qos': 'SENSOR_DATA',
        'depth_module.depth_qos': 'SENSOR_DATA',
        'align_depth.enable': 'true',
        'enable_sync': 'true',
        'pointcloud.enable': 'false', 
        'decimation_filter.enable': 'true', # Reduces depth resolution by 2x (very fast)
        'spatial_filter.enable': 'true',    # Smooths depth
        'temporal_filter.enable': 'true',   # Stabilizes depth
    }

    # Include the official launch file with our overrides
    rs_launch = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(os.path.join(realsense_dir, 'launch', 'rs_launch.py')),
        launch_arguments=launch_args.items()
    )

    return LaunchDescription([
        rs_launch
    ])
