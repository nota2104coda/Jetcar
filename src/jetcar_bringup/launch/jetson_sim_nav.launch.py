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
            'stop_button_state': False
        }]
    )

    # 2. HMI Node
    hmi_node = Node(
        package='jetcar_common',
        executable='hmi_node',
        name='hmi_node',
        output='screen',
        parameters=[{'use_sim_time': True}]
    )

    # 3. Navigation Stack (Nav2)
    navigation_launch = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(os.path.join(pkg_nav, 'launch', 'navigation.launch.py')),
        launch_arguments={
            'use_sim_time': 'true',
            'params_file': os.path.join(pkg_nav, 'config', 'nav2_params_sim.yaml')
        }.items()
    )

    return LaunchDescription([
        sim_mcu_node,
        hmi_node,
        navigation_launch
    ])
