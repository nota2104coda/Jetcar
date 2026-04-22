import os
from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import TimerAction, LogInfo
from launch_ros.actions import Node, ComposableNodeContainer
from launch_ros.descriptions import ComposableNode

def generate_launch_description():
    pkg_jetson_cpp = get_package_share_directory('jetsoncpp_pkg')
    nav2_params = os.path.join(pkg_jetson_cpp, 'config', 'nav2_params_sim.yaml')
    
    # 1. nvblox (Configured for Gazebo sensor topics)
    nvblox_container = ComposableNodeContainer(
        name='nvblox_container',
        namespace='',
        package='rclcpp_components',
        executable='component_container_mt',
        composable_node_descriptions=[
            ComposableNode(
                name='nvblox_node',
                package='nvblox_ros',
                plugin='nvblox::NvbloxNode',
                parameters=[{
                    'global_frame': 'odom',
                    'use_sim_time': True,
                    'voxel_size': 0.1,
                    'use_static_occupancy_layer': True,
                    'publish_esdf_distance_slice': True,
                }],
                remappings=[
                    ('/camera_0/depth/image', '/camera/depth/image_rect_raw'),
                    ('/camera_0/depth/camera_info', '/camera/depth/camera_info'),
                    ('/camera_0/color/image', '/camera/color/image_raw'),
                    ('/camera_0/color/camera_info', '/camera/color/camera_info'),
                    ('pose', '/odom'), # Using Gazebo Ground Truth Odom for stability
                ]
            ),
        ],
        output='screen'
    )

    # 2. Nav2 Stack
    nav2_nodes = [
        Node(
            package='nav2_controller', 
            executable='controller_server', 
            name='controller_server', 
            parameters=[nav2_params],
            remappings=[('cmd_vel', 'cmd_vel_nav')]
        ),
        Node(package='nav2_planner', executable='planner_server', name='planner_server', parameters=[nav2_params]),
        Node(package='nav2_behaviors', executable='behavior_server', name='behavior_server', parameters=[nav2_params]),
        Node(package='nav2_bt_navigator', executable='bt_navigator', name='bt_navigator', parameters=[nav2_params]),
        Node(package='nav2_lifecycle_manager', executable='lifecycle_manager', name='lifecycle_manager_navigation',
             parameters=[{'use_sim_time': True, 'autostart': True, 'node_names': ['controller_server', 'planner_server', 'behavior_server', 'bt_navigator']}]),
    ]

    # 3. Sim MCU Node
    sim_mcu = Node(
        package='jetsoncpp_pkg',
        executable='sim_mcu_node',
        name='sim_mcu_node',
        parameters=[{
            'use_sim_time': True,
        }]
    )

    # 3. Your Custom Camera Node (Adaptive Resolution)
    adaptive_node = Node(
        package='jetsoncpp_pkg',
        executable='adaptive_resolution_node',
        name='adaptive_resolution',
        parameters=[{
            'use_sim_time': True,
            'camera_node_name': '/camera/camera', # Note: Gazebo plugin namespace
            'low_speed_threshold': 0.1,
            'high_speed_threshold': 0.2,
        }]
    )

    return LaunchDescription([
        nvblox_container,
        sim_mcu,
        adaptive_node,
        *nav2_nodes
    ])
