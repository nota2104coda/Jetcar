import os
from launch import LaunchDescription
from launch.actions import IncludeLaunchDescription, DeclareLaunchArgument
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration
from ament_index_python.packages import get_package_share_directory

def generate_launch_description():
    pkg_bringup = get_package_share_directory('jetcar_bringup')
    pkg_sim = get_package_share_directory('jetcar_sim')
    pkg_nav = get_package_share_directory('jetcar_nav')

    # 1. Gazebo Simulation (Gazebo, Robot State Publisher, World)
    gazebo_launch = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(os.path.join(pkg_sim, 'launch', 'gazebo.launch.py'))
    )

    # 2. Navigation Stack (Nav2)
    # Use the simulation params
    navigation_launch = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(os.path.join(pkg_nav, 'launch', 'navigation.launch.py')),
        launch_arguments={
            'use_sim_time': 'true',
            'params_file': os.path.join(pkg_nav, 'config', 'nav2_params_sim.yaml')
        }.items()
    )

    # 3. SLAM Toolbox (for map -> odom TF)
    from launch_ros.actions import Node
    slam_toolbox_node = Node(
        package='slam_toolbox',
        executable='async_slam_toolbox_node',
        name='slam_toolbox',
        parameters=[{
            'use_sim_time': True,
            'odom_frame': 'odom',
            'base_frame': 'base_link',
            'map_frame': 'map',
            'scan_topic': '/scan',
            'mode': 'mapping',
        }]
    )

    return LaunchDescription([
        gazebo_launch,
        slam_toolbox_node,
        navigation_launch
    ])
