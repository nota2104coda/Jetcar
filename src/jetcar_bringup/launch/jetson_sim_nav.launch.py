import os
from launch import LaunchDescription
from launch.actions import IncludeLaunchDescription
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch_ros.actions import Node
from ament_index_python.packages import get_package_share_directory

def generate_launch_description():
    pkg_nav = get_package_share_directory('jetcar_nav')

    # 1. Simulation MCU Node
    sim_mcu_node = Node(
        package='jetcar_sim',
        executable='sim_mcu_node',
        output='screen',
        parameters=[{
            'use_sim_time': True,
            'stop_button_state': False,
            'manual_scale': 1.0,
            'auto_scale': 1.0,
        }]
    )

    # 2. HMI Node
    hmi_node = Node(
        package='jetcar_common',
        executable='hmi_node_nobridge',
        name='hmi_node',
        output='screen',
        parameters=[{'use_sim_time': True}]
    )

    # 3. SLAM Toolbox (for map -> odom TF)
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

    # 4. Navigation Stack (Nav2)
    navigation_launch = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(os.path.join(pkg_nav, 'launch', 'navigation.launch.py')),
        launch_arguments={
            'use_sim_time': 'true',
            'params_file': os.path.join(pkg_nav, 'config', 'nav2_params_sim.yaml')
        }.items()
    )

    # 5. Foxglove Bridge (for visualization)
    foxglove_bridge = Node(
        package='foxglove_bridge',
        executable='foxglove_bridge',
        name='foxglove_bridge',
        parameters=[{'use_sim_time': True}]
    )

    return LaunchDescription([
        sim_mcu_node,
        hmi_node,
        slam_toolbox_node,
        navigation_launch,
        foxglove_bridge
    ])
