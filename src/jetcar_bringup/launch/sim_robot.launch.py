import os
from launch import LaunchDescription
from launch.actions import IncludeLaunchDescription, TimerAction, LogInfo
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch_ros.actions import Node, ComposableNodeContainer, LoadComposableNodes, SetParameter
from launch_ros.descriptions import ComposableNode
from ament_index_python.packages import get_package_share_directory

def generate_launch_description():
    pkg_bringup = get_package_share_directory('jetcar_bringup')
    pkg_sim = get_package_share_directory('jetcar_sim')
    pkg_nav = get_package_share_directory('jetcar_nav')
    pkg_common = get_package_share_directory('jetcar_common')
    # 0. HMI Node (Common)
    hmi_node = Node(
        package='jetcar_common',
        executable='hmi_node_nobridge',
        name='hmi_node'
    )

    # Robot State Publisher (Required locally for tf_static across network)
    pkg_description = get_package_share_directory('jetcar_description')
    from launch.substitutions import Command
    xacro_path = os.path.join(pkg_description, 'urdf', 'jetcar.urdf.xacro')
    robot_description_content = Command(['xacro ', xacro_path, ' sim_mode:=true'])

    robot_state_publisher = Node(
        package='robot_state_publisher',
        executable='robot_state_publisher',
        name='robot_state_publisher_jetson',
        output='screen',
        parameters=[{
            'robot_description': robot_description_content,
            'use_sim_time': True
        }]
    )

    # 1. Isaac ROS Container (Zero-Copy Vision Stack)
    isaac_container = ComposableNodeContainer(
        name='isaac_ros_container',
        namespace='',
        package='rclcpp_components',
        executable='component_container_mt',
        output='screen',
    )

    # 2. Visual SLAM (Delayed 5s)
    load_vslam = LoadComposableNodes(
        target_container='isaac_ros_container',
        composable_node_descriptions=[
            ComposableNode(
                name='visual_slam',
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

    # 3. Image Format Converter & nvblox (Delayed)
    load_nvblox = LoadComposableNodes(
        target_container='isaac_ros_container',
        composable_node_descriptions=[
            ComposableNode(
                name='image_format_converter_node',
                package='isaac_ros_image_proc',
                plugin='nvidia::isaac_ros::image_proc::ImageFormatConverterNode',
                parameters=[{'encoding_desired': 'rgb8'}],
                remappings=[
                    ('image', '/camera/camera/color/image_rgb'),
                    ('image_raw', '/camera/camera/color/image_raw'),
                ]
            ),
            ComposableNode(
                name='nvblox_node',
                package='nvblox_ros',
                plugin='nvblox::NvbloxNode',
                parameters=[{
                    'global_frame': 'map',
                    'voxel_size': 0.1,
                    'use_static_occupancy_layer': True,
                    # 'use_depth': True,
                    # 'use_lidar': False,
                    # 'use_color': True,
                }],
                remappings=[
                    ('depth/image', '/camera/camera/depth/image_rect_raw'),
                    ('depth/camera_info', '/camera/camera/depth/camera_info'),
                    ('color/image', '/camera/camera/color/image_rgb'),
                    ('color/camera_info', '/camera/camera/color/camera_info'),
                    ('pose', '/visual_slam/tracking/vo_pose'),
                ]
            ),
        ]
    )

    # 4. Navigation & Localization (Unified)
    # This handles EKF, Nav2, and SLAM (because slam:=true)
    navigation_launch = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(os.path.join(pkg_nav, 'launch', 'navigation.launch.py')),
        launch_arguments={
            'use_sim_time': 'True',
            'params_file': os.path.join(pkg_nav, 'config', 'nav2_params_sim.yaml'),
            'slam': 'True'
        }.items()
    )

    # 5. Utils
    foxglove_bridge = Node(
        package='foxglove_bridge',
        executable='foxglove_bridge',
        name='foxglove_bridge',
        parameters=[{'use_sim_time': True}]
    )

    return LaunchDescription([
        # CRITICAL: This globally sets use_sim_time=True for all nodes in this launch file!
        SetParameter('use_sim_time', True),
        
        hmi_node,
        robot_state_publisher,
        isaac_container,
        TimerAction(period=5.0, actions=[LogInfo(msg='Loading Visual SLAM...'), load_vslam]),
        TimerAction(period=8.0, actions=[LogInfo(msg='Loading nvblox...'), load_nvblox]),
        navigation_launch,
        foxglove_bridge
    ])
