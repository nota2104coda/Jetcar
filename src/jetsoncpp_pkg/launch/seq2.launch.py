import os
from launch import LaunchDescription
from launch.actions import IncludeLaunchDescription, DeclareLaunchArgument, TimerAction, LogInfo
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import Command, LaunchConfiguration
from launch_ros.actions import ComposableNodeContainer, Node, LoadComposableNodes
from launch_ros.descriptions import ComposableNode
from ament_index_python.packages import get_package_share_directory

def generate_launch_description():
    pkg_jetson_cpp = get_package_share_directory('jetsoncpp_pkg')
    nav2_params_path = os.path.join(pkg_jetson_cpp, 'config', 'nav2_params.yaml')
    nav2_bt_share = get_package_share_directory('nav2_bt_navigator')
    nav2_nav_pose_bt = os.path.join(
        nav2_bt_share,
        'behavior_trees',
        'navigate_to_pose_w_replanning_and_recovery.xml'
    )
    nav2_nav_through_bt = os.path.join(
        nav2_bt_share,
        'behavior_trees',
        'navigate_through_poses_w_replanning_and_recovery.xml'
    )
    
    # 0. Robot State Publisher (URDF)
    urdf_path = os.path.join(pkg_jetson_cpp, 'urdf', 'skid_steer_4wd.urdf')
    robot_description_content = Command(['xacro ', urdf_path])
    
    robot_state_publisher_node = Node(
        package='robot_state_publisher',
        executable='robot_state_publisher',
        name='robot_state_publisher',
        output='screen',
        parameters=[{'robot_description': robot_description_content}]
    )

    # 1. Isaac ROS Container
    container = ComposableNodeContainer(
        name='isaac_ros_container',
        namespace='',
        package='rclcpp_components',
        executable='component_container_mt',
        output='screen',
        composable_node_descriptions=[
            # A. RealSense Camera Node
            ComposableNode(
                package='realsense2_camera',
                plugin='realsense2_camera::RealSenseNodeFactory',
                name='camera',
                namespace='camera',
                parameters=[{
                    'rgb_camera.color_profile': '424x240x30', 
                    'depth_module.depth_profile': '424x240x30',
                    'depth_module.infra_profile': '424x240x30',
                    'rgb_camera.color_format': 'BGR8',
                    'rgb_camera.color_qos': 'RELIABLE', # Changed to RELIABLE for Foxglove
                    'depth_module.depth_qos': 'SENSOR_DATA',
                    'enable_infra1': True,
                    'enable_infra2': True, 
                    'enable_depth': True, 
                    'enable_color': True,
                    'enable_sync': True,
                    'frames_queue_size': 2,
                }]
            ),
        ]
    )

    # 2. Sequential Loading Actions
    # 2.1 Visual SLAM (Loaded after 5s)
    load_vslam = LoadComposableNodes(
        target_container='isaac_ros_container',
        composable_node_descriptions=[
            ComposableNode(
                name='visual_slam_node',
                package='isaac_ros_visual_slam',
                plugin='nvidia::isaac_ros::visual_slam::VisualSlamNode',
                parameters=[{
                    'enable_imu_fusion': True, 
                    'imu_frame': 'base_link',
                    'base_frame': 'base_link',
                    'odom_frame': 'odom',
                    'map_frame': 'map',
                    'publish_odom_to_base_tf': False,
                    'publish_map_to_odom_tf': False,
                    'max_delta_frame_time_ms': 100.0,
                    'min_delta_frame_time_ms': 0.0,
                    'input_imu_frame_rate': 200.0,
                }],
                remappings=[
                    ('visual_slam/image_0', '/camera/camera/infra1/image_rect_raw'),
                    ('visual_slam/camera_info_0', '/camera/camera/infra1/camera_info'),
                    ('visual_slam/image_1', '/camera/camera/infra2/image_rect_raw'),
                    ('visual_slam/camera_info_1', '/camera/camera/infra2/camera_info'),
                    ('visual_slam/imu', '/mcu/imu'), 
                ]
            ),
        ]
    )

    # 2.2 Image Format Converter (BGR8 -> RGB8 for nvblox)
    load_converter = LoadComposableNodes(
        target_container='isaac_ros_container',
        composable_node_descriptions=[
            ComposableNode(
                name='image_format_converter_node',
                package='isaac_ros_image_proc',
                plugin='nvidia::isaac_ros::image_proc::ImageFormatConverterNode',
                parameters=[{
                    'encoding_desired': 'rgb8',
                }],
                remappings=[
                    ('image', '/camera/camera/color/image_rgb'), # Output topic
                    ('image_raw', '/camera/camera/color/image_raw'), # Input topic
                ]
            ),
        ]
    )

    # 2.3 nvblox (Loaded after 8s - depends on SLAM for odometry and Converter for RGB8)
    load_nvblox = LoadComposableNodes(
        target_container='isaac_ros_container',
        composable_node_descriptions=[
            ComposableNode(
                name='nvblox_node',
                package='nvblox_ros',
                plugin='nvblox::NvbloxNode',
                parameters=[{
                    'global_frame': 'map',
                    'voxel_size': 0.1,
                    'use_static_occupancy_layer': True,
                    'use_color': True,
                    'use_depth': True,
                    'compute_mesh': True,
                    'mesh_update_period_ms': 200,
                    'publish_tsdf_marker': True,
                    'tsdf_marker_update_period_ms': 200,
                    'publish_color_layer_marker': True,
                    'color_layer_marker_update_period_ms': 200,
                    'publish_esdf_distance_slice': True,
                    'publish_static_map': True,
                }],
                remappings=[
                    ('/camera_0/depth/image', '/camera/camera/depth/image_rect_raw'),
                    ('/camera_0/depth/camera_info', '/camera/camera/depth/camera_info'),
                    ('/camera_0/color/image', '/camera/camera/color/image_rgb'),
                    ('/camera_0/color/camera_info', '/camera/camera/color/camera_info'),
                    ('pose', '/visual_slam/tracking/vo_pose'),
                ]
            ),
        ]
    )

    delayed_vslam = TimerAction(
        period=5.0,
        actions=[
            LogInfo(msg='[Sequential Launch] Loading Visual SLAM...'),
            load_vslam
        ]
    )

    delayed_converter = TimerAction(
        period=6.0,
        actions=[
            LogInfo(msg='[Sequential Launch] Loading Image Format Converter...'),
            load_converter
        ]
    )

    delayed_nvblox = TimerAction(
        period=8.0,
        actions=[
            LogInfo(msg='[Sequential Launch] Loading nvblox 3D reconstruction...'),
            load_nvblox
        ]
    )

    # 3. Support Nodes
    ld06_node = Node(
        package='ldlidar_ros2',
        executable='ldlidar_ros2_node',
        name='ld06_lidar',
        parameters=[
            {'product_name': 'LDLiDAR_LD06'},
            {'laser_scan_topic_name': 'scan'},
            {'point_cloud_2d_topic_name': 'pointcloud2d'},
            {'port_name': '/dev/serial/by-id/usb-Silicon_Labs_CP2104_USB_to_UART_Bridge_Controller_024TKTOY-if00-port0'},
            {'serial_baudrate': 230400},
            {'frame_id': 'ld06_lidar'}, 
            {'laser_scan_dir': True},
            {'range_min': 0.02},
            {'range_max': 12.0}
        ]
    )
    
    lidar_pwm_node = Node(
        package='jetsoncpp_pkg',
        executable='lidar_pwm.py',
        name='lidar_pwm_control'
    )

    hw_mcu_node = Node(
        package='jetsoncpp_pkg',
        executable='hw_mcu_node',
        name='hw_mcu_node',
        parameters=[{
            'serial_port': '/dev/serial/by-id/usb-Silicon_Labs_CP2104_USB_to_UART_Bridge_Controller_02CZJZRS-if00-port0', 
            'serial_baud_rate': 1000000,
            'enable_tf_broadcast': False,
            'poll_period': 0.005,    # 200Hz for SLAM
            'control_period': 0.02,  # 50Hz control
            'command_topic_manual': 'cmd_vel_manual',
            'command_topic_nav': 'cmd_vel_nav',
        }]
    )

    ekf_config_path = os.path.join(pkg_jetson_cpp, 'config', 'ekf.yaml')
    ekf_node = Node(
        package='robot_localization',
        executable='ekf_node',
        name='ekf_filter_node',
        parameters=[ekf_config_path]
    )

    slam_toolbox_node = Node(
        package='slam_toolbox',
        executable='async_slam_toolbox_node',
        name='slam_toolbox',
        parameters=[{
            'use_sim_time': False,
            'odom_frame': 'odom',
            'base_frame': 'base_link',
            'map_frame': 'map',
            'scan_topic': '/scan',
            'mode': 'mapping',
        }]
    )

    # 7. Nav2 Stack (planner, controller, BT navigator)
    nav2_controller_node = Node(
        package='nav2_controller',
        executable='controller_server',
        name='controller_server',
        output='screen',
        parameters=[nav2_params_path],
        remappings=[
            ('cmd_vel', 'cmd_vel_nav_raw'),
        ]
    )

    nav2_planner_node = Node(
        package='nav2_planner',
        executable='planner_server',
        name='planner_server',
        output='screen',
        parameters=[nav2_params_path]
    )

    nav2_smoother_node = Node(
        package='nav2_smoother',
        executable='smoother_server',
        name='smoother_server',
        output='screen',
        parameters=[nav2_params_path]
    )

    nav2_behavior_node = Node(
        package='nav2_behaviors',
        executable='behavior_server',
        name='behavior_server',
        output='screen',
        parameters=[nav2_params_path]
    )

    nav2_bt_navigator_node = Node(
        package='nav2_bt_navigator',
        executable='bt_navigator',
        name='bt_navigator',
        output='screen',
        parameters=[
            nav2_params_path,
            {
                'default_nav_to_pose_bt_xml': nav2_nav_pose_bt,
                'default_nav_through_poses_bt_xml': nav2_nav_through_bt,
            }
        ],
        remappings=[
            ('goal_pose', '/move_base_simple/goal'),
        ]
    )

    nav2_waypoint_node = Node(
        package='nav2_waypoint_follower',
        executable='waypoint_follower',
        name='waypoint_follower',
        output='screen',
        parameters=[nav2_params_path]
    )

    nav2_velocity_smoother_node = Node(
        package='nav2_velocity_smoother',
        executable='velocity_smoother',
        name='velocity_smoother',
        output='screen',
        parameters=[nav2_params_path],
        remappings=[
            ('cmd_vel', 'cmd_vel_nav_raw'),
            ('cmd_vel_smoothed', 'cmd_vel_nav'),
        ]
    )

    nav2_lifecycle_manager_node = Node(
        package='nav2_lifecycle_manager',
        executable='lifecycle_manager',
        name='lifecycle_manager_navigation',
        output='screen',
        parameters=[{
            'use_sim_time': False,
            'autostart': True,
            'bond_timeout': 10.0,
            'node_names': [
                'controller_server',
                'planner_server',
                'smoother_server',
                'behavior_server',
                'bt_navigator',
                'waypoint_follower',
                'velocity_smoother',
            ],
        }]
    )

    hmi_node = Node(
        package='jetsoncpp_pkg',
        executable='hmi_node_nobridge',
        name='hmi_node'
    )

    foxglove_bridge_node = Node(
        package='foxglove_bridge',
        executable='foxglove_bridge',
        name='foxglove_bridge'
    )

    return LaunchDescription([
        robot_state_publisher_node,
        container,
        delayed_vslam,
        delayed_converter,
        delayed_nvblox,
        ld06_node,
        lidar_pwm_node,
        hw_mcu_node,
        ekf_node,
        slam_toolbox_node,
        nav2_controller_node,
        nav2_planner_node,
        nav2_smoother_node,
        nav2_behavior_node,
        nav2_bt_navigator_node,
        nav2_waypoint_node,
        nav2_velocity_smoother_node,
        nav2_lifecycle_manager_node,
        hmi_node,
        foxglove_bridge_node
    ])
