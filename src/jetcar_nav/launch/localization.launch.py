import os
from launch import LaunchDescription
from launch.actions import IncludeLaunchDescription, DeclareLaunchArgument
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration
from ament_index_python.packages import get_package_share_directory

def generate_launch_description():
    """
    This launch file is now a wrapper around navigation.launch.py 
    specifically configured for SLAM (Mapping) mode.
    """
    pkg_nav = get_package_share_directory('jetcar_nav')
    
    use_sim_time = LaunchConfiguration('use_sim_time')
    params_file = LaunchConfiguration('params_file')

    navigation_launch = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(os.path.join(pkg_nav, 'launch', 'navigation.launch.py')),
        launch_arguments={
            'use_sim_time': use_sim_time,
            'params_file': params_file,
            'slam': 'True',
        }.items()
    )

    return LaunchDescription([
        DeclareLaunchArgument(
            'use_sim_time',
            default_value='false',
            description='Use simulation (Gazebo) clock if true'),
        DeclareLaunchArgument(
            'params_file',
            default_value=os.path.join(pkg_nav, 'config', 'nav2_params.yaml'),
            description='Full path to the ROS2 parameters file to use'),
        navigation_launch
    ])
