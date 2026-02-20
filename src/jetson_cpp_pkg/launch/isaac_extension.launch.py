import os
from launch import LaunchDescription
from launch.actions import IncludeLaunchDescription, DeclareLaunchArgument
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch_ros.actions import ComposableNodeContainer, Node
from launch_ros.descriptions import ComposableNode
from ament_index_python.packages import get_package_share_directory

def generate_launch_description():
    # 1. Start a NITROS-capable container
    # This container will host both the RealSense node (if composable) and Isaac ROS nodes
    # For zero-copy NITROS, nodes must be in the same component container.
    
    # Note: realsense2_camera uses its own container by default in its launch file.
    # To use NITROS properly, we should load RealSense as a component into our container.
    
    container = ComposableNodeContainer(
        name='isaac_ros_container',
        namespace='',
        package='rclcpp_components',
        executable='component_container_mt',
        output='screen',
        composable_node_descriptions=[
            # A. RealSense Camera Node (as component)
            ComposableNode(
                package='realsense2_camera',
                plugin='realsense2_camera::RealSenseNodeFactory',
                name='camera',
                namespace='camera',
                parameters=[{
                    'rgb_camera.color_profile': '640x480x15',
                    'depth_module.depth_profile': '640x480x15',
                    'depth_module.infra_profile': '640x480x15', # Matching for stability
                    'rgb_camera.color_qos': 'SENSOR_DATA',
                    'depth_module.depth_qos': 'SENSOR_DATA',
                    
                    'align_depth.enable': False, # Processing heavy, disable unless strict RGB-D alignment needed
                    'enable_sync': True,
                    'pointcloud.enable': False, # Bandwidth heavy, use depth image for obstacles if possible
                    
                    # Golden Mean:
                    # 1. Enable Infra 1+2 for VSLAM (Essential)
                    'enable_infra1': True,
                    'enable_infra2': True,
                    # 2. Enable Depth for Obstacle Avoidance (Optional but standard)
                    'enable_depth': True, 
                    # 3. Enable Color for Teleop (Low res)
                    'enable_color': True,

                    'decimation_filter.enable': True, # Reduces depth spread
                    'spatial_filter.enable': True,
                    'temporal_filter.enable': True,
                    
                    # CRITICAL: Emitter ON is good for Depth (Wall texture), 
                    # but BAD for VSLAM (moving dots confuse tracker).
                    # '1' is "Laser", '0' is "Off". 
                    # If VSLAM is priority, set to Off. If Wall avoidance is priority, set to On.
                    'emitter_enabled': False, # Prioritize VSLAM stability
                }],
                extra_arguments=[{'use_intra_process_comms': True}]
            ),
            
            # B. Isaac ROS Visual SLAM Node
            ComposableNode(
                name='visual_slam_node',
                package='isaac_ros_visual_slam',
                plugin='nvidia::isaac_ros::visual_slam::VisualSlamNode',
                parameters=[{
                    'enable_image_denoising': False,
                    'rectified_images': True,
                    'enable_imu_fusion': True, # Use IMU
                    'imu_frame': 'base_link', # Assuming MCU IMU is in base_link or we have a transform
                    'base_frame': 'base_link',
                    'odom_frame': 'odom',
                    'map_frame': 'map',
                }],
                remappings=[
                    # Subscribe to RealSense Stereo IR images
                    ('visual_slam/image_0', '/camera/camera/infra1/image_rect_raw'),
                    ('visual_slam/camera_info_0', '/camera/camera/infra1/camera_info'),
                    ('visual_slam/image_1', '/camera/camera/infra2/image_rect_raw'),
                    ('visual_slam/camera_info_1', '/camera/camera/infra2/camera_info'),
                    # Subscribe to MCU IMU
                    ('visual_slam/imu', '/mcu/imu'), 
                ]
            ),

            # C. Isaac ROS Resize Node (Example NITROS processing)
            # This node takes the raw color image and resizes it using GPU
            # Input: /camera/camera/color/image_raw
            # Output: /camera/camera/color/image_resized
            ComposableNode(
                package='isaac_ros_image_proc',
                plugin='nvidia::isaac_ros::image_proc::ResizeNode',
                name='isaac_ros_resize',
                namespace='camera',
                parameters=[{
                    'output_width': 320,
                    'output_height': 240,
                    'num_blocks': 40, # Block tuning for performance
                }],
                remappings=[
                    ('image', 'camera/color/image_raw'),
                    ('camera_info', 'camera/color/camera_info'),
                    ('resize/image', 'camera/color/image_resized'),
                    ('resize/camera_info', 'camera/color/camera_info_resized'),
                ],
                # Disable strict intra-process comms for resize due to QoS durability mismatch risks
                # The RealSense node publishes SENSOR_DATA (Best Effort/Volatile), but sometimes
                # defaults can cause the "durability" check to fail during INIT.
                # NITROS will still use zero-copy if available via shared memory.
                extra_arguments=[{'use_intra_process_comms': False}]
            ),
            
            # C. Isaac ROS Rectify Node (Optional, typically used after resize)
            # Uses NITROS if available
            # ComposableNode(
            #     package='isaac_ros_image_proc',
            #     plugin='nvidia::isaac_ros::image_proc::RectifyNode',
            #     name='isaac_ros_rectify',
            #     namespace='camera',
            #     remappings=[
            #         ('image_raw', 'camera/color/image_resized'),
            #         ('camera_info', 'camera/color/camera_info_resized'),
            #         ('image_rect', 'camera/color/image_rect'),
            #         ('camera_info_rect', 'camera/color/camera_info_rect'),
            #     ],
            #     extra_arguments=[{'use_intra_process_comms': True}]
            # )
        ]
    )

    # 2. LD06 Lidar Node (Standard Node)
    # This does NOT need to be a ComposableNode because:
    # a. The driver likely doesn't support zero-copy component interface.
    # b. Lidar data (LaserScan) bandwidth is very low compared to finding images.
    # c. The overhead of standard IPC is negligible here.
    ld06_node = Node(
        package='ldlidar_ros2',
        executable='ldlidar_ros2_node',
        name='ld06_lidar',
        output='screen',
        parameters=[
            {'product_name': 'LDLiDAR_LD06'},
            {'laser_scan_topic_name': 'scan'},
            {'point_cloud_2d_topic_name': 'pointcloud2d'},
            {'port_name': '/dev/ttyTHS1'},
            {'serial_baudrate': 230400},
            {'frame_id': 'laser_frame'},
            {'laser_scan_dir': True},
            {'enable_angle_crop_func': False},
            {'angle_crop_min': 135.0},
            {'angle_crop_max': 225.0},
            {'range_min': 0.02},
            {'range_max': 12.0}
        ]
    )

    # 3. Static TF for Lidar
    # Adjust x/y/z/roll/pitch/yaw to match your robot
    lidar_tf = Node(
        package='tf2_ros',
        executable='static_transform_publisher',
        name='lidar_tf',
        arguments=['0', '0', '0.1', '0', '0', '0', 'base_link', 'laser_frame']
    )

    return LaunchDescription([
        container,
        ld06_node,
        lidar_tf
    ])
