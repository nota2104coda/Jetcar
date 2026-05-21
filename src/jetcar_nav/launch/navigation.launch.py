import os
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, IncludeLaunchDescription, GroupAction
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node, SetRemap
from ament_index_python.packages import get_package_share_directory

def generate_launch_description():
    pkg_nav = get_package_share_directory('jetcar_nav')
    pkg_common = get_package_share_directory('jetcar_common')
    pkg_nav2_bringup = get_package_share_directory('nav2_bringup')
    
    # Launch Configurations
    use_sim_time = LaunchConfiguration('use_sim_time')
    params_file = LaunchConfiguration('params_file')
    slam = LaunchConfiguration('slam')
    map_yaml_file = LaunchConfiguration('map')
    autostart = LaunchConfiguration('autostart')

    # 1. EKF Node for Odometry Filtering
    ekf_config_path = os.path.join(pkg_common, 'config', 'ekf.yaml')
    ekf_node = Node(
        package='robot_localization',
        executable='ekf_node',
        name='ekf_filter_node',
        parameters=[ekf_config_path, {'use_sim_time': use_sim_time}],
        remappings=[('/odometry/filtered','/odom')],
        output='screen'
    )

    # 2. Standard Nav2 Bringup (handles Planner, Controller, BT, etc.)
    # We remap the final output topics to /cmd_vel_nav
    nav2_bringup_launch = GroupAction(
        actions=[
            SetRemap(src='cmd_vel', dst='/cmd_vel_nav'),
            SetRemap(src='cmd_vel_smoothed', dst='/cmd_vel_nav'),
            IncludeLaunchDescription(
                PythonLaunchDescriptionSource(os.path.join(pkg_nav2_bringup, 'launch', 'bringup_launch.py')),
                launch_arguments={
                    'use_sim_time': use_sim_time,
                    'params_file': params_file,
                    'slam': slam,
                    'map': map_yaml_file,
                    'autostart': autostart,
                    'use_composition': 'False',
                }.items(),
            )
        ]
    )

    # Goal Fixer Relay (Fixes 0 timestamps from Foxglove and avoids topic_tools dependency)
    goal_pose_relay = Node(
        package='jetcar_common',
        executable='goal_fixer.py',
        name='goal_pose_relay',
        parameters=[{'use_sim_time': use_sim_time}]
    )

    return LaunchDescription([
        DeclareLaunchArgument(
            'use_sim_time',
            default_value='False',
            description='Use simulation (Gazebo) clock if true'),
        
        DeclareLaunchArgument(
            'params_file',
            default_value=os.path.join(pkg_nav, 'config', 'nav2_params.yaml'),
            description='Full path to the ROS2 parameters file to use for all launched nodes'),
        
        DeclareLaunchArgument(
            'slam',
            default_value='False',
            description='Whether to run SLAM'),
        
        DeclareLaunchArgument(
            'map',
            default_value='',
            description='Full path to map yaml file to load'),
            
        DeclareLaunchArgument(
            'autostart',
            default_value='True',
            description='Automatically startup the nav2 stack'),

        ekf_node,
        nav2_bringup_launch,
        goal_pose_relay
    ])
