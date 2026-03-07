import os
from launch import LaunchDescription
from launch.actions import IncludeLaunchDescription, DeclareLaunchArgument
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import Command
from launch_ros.actions import ComposableNodeContainer, Node
from launch_ros.descriptions import ComposableNode
from ament_index_python.packages import get_package_share_directory

def generate_launch_description():
    pkg_jetson_cpp = get_package_share_directory('jetsoncpp_pkg')

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

    # 1. Start a NITROS-capable container
    # This container will host both the RealSense node (if composable) and Isaac ROS nodes
    # For zero-copy NITROS, nodes must be in the same component container.
    
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
                    'depth_module.depth_profile': '424x240x15',
                    'depth_module.infra_profile': '424x240x15',
                    'rgb_camera.color_qos': 'SENSOR_DATA',
                    'depth_module.depth_qos': 'SENSOR_DATA',
                    'align_depth.enable': False, # Processing heavy, disable unless strict RGB-D alignment needed
                    'frames_queue_size': 10,
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
                    'decimation_filter.enable': False, # Reduces depth spread but adds latency, disable for now
                    'spatial_filter.enable': False, # Reduces depth noise but adds latency, disable for now
                    'temporal_filter.enable': False, # Reduces depth noise over time but adds latency, disable for now
                    #can keep Emitter in auto, but disabled for now. if True, then can cause noisier depth readings in well lit rooms.
                    'emitter_enabled': False, 
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
                    'enable_imu_fusion': True, 
                    'imu_frame': 'base_link', #tells system the IMU is fixed to base, not moving with an arm.can set this to IMU link too.
                    'base_frame': 'base_link',
                    'odom_frame': 'odom',
                    'map_frame': 'map',
                }],
                remappings=[
                    ('visual_slam/image_0', '/camera/camera/infra1/image_rect_raw'),
                    ('visual_slam/camera_info_0', '/camera/camera/infra1/camera_info'),
                    ('visual_slam/image_1', '/camera/camera/infra2/image_rect_raw'),
                    ('visual_slam/camera_info_1', '/camera/camera/infra2/camera_info'),
                    ('visual_slam/imu', '/mcu/imu'), 
                ],
                extra_arguments=[{'use_intra_process_comms': True}]
            ),

            # C. Isaac ROS Resize Node
            ComposableNode(
                package='isaac_ros_image_proc',
                plugin='nvidia::isaac_ros::image_proc::ResizeNode',
                name='isaac_ros_resize',
                namespace='camera',
                parameters=[{
                    'output_width': 320,
                    'output_height': 240,
                    'num_blocks': 40,# Block tuning for performance
                }],
                remappings=[
                    ('image', 'camera/color/image_raw'),
                    ('camera_info', 'camera/color/camera_info'),
                    ('resize/image', 'camera/color/image_resized'),
                    ('resize/camera_info', 'camera/color/camera_info_resized'),
                ],
                extra_arguments=[{'use_intra_process_comms': True}]
            ),
        ]
    )

    # 2. LD06 Lidar Node
    ld06_node = Node(
        package='ldlidar_ros2',
        executable='ldlidar_ros2_node',
        name='ld06_lidar',
        output='screen',
        parameters=[
            {'product_name': 'LDLiDAR_LD06'},
            {'laser_scan_topic_name': 'scan'},
            {'point_cloud_2d_topic_name': 'pointcloud2d'},
            {'port_name': '/dev/serial/by-id/usb-Silicon_Labs_CP2104_USB_to_UART_Bridge_Controller_024TKTOY-if00-port0'},
            {'serial_baudrate': 230400},
            {'frame_id': 'ld06_lidar'}, 
            {'laser_scan_dir': True},
            #use below if lidar is obstructed in a certain range of angle. for e.g. if mounted in front of the car.
            {'enable_angle_crop_func': False},
            {'angle_crop_min': 135.0},
            {'angle_crop_max': 225.0},
            
            {'range_min': 0.02},
            {'range_max': 12.0}
        ]
    )

    # 3. HMI Node
    hmi_node = Node(
        package='jetsoncpp_pkg',
        executable='hmi_node',
        name='hmi_node',
        output='screen',
        parameters=[{'use_sim_time': False}]
    )

    # 4. Hardware MCU Node
    hw_mcu_node = Node(
        package='jetsoncpp_pkg',
        executable='hw_mcu_node',
        name='hw_mcu_node',
        output='screen',
        parameters=[{
            'serial_port': '/dev/serial/by-id/usb-Silicon_Labs_CP2104_USB_to_UART_Bridge_Controller_02CZJZRS-if00-port0', 
            'serial_baud_rate': 921600,
            'poll_period': 0.01,
            'control_period': 0.05
        }]
    )

    return LaunchDescription([
        robot_state_publisher_node,
        container,
        ld06_node,
        hmi_node,
        hw_mcu_node
    ])
