import os
from launch import LaunchDescription
from launch.actions import IncludeLaunchDescription, DeclareLaunchArgument
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import Command, LaunchConfiguration
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

    # 1. Isaac ROS Container (Realsense + VSLAM)
    # Configure VSLAM to publish odometry but NOT map->odom TF (let SLAM toolbox do that)
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
                    'rgb_camera.color_profile': '424x240x15', 
                    'depth_module.depth_profile': '424x240x15',
                    'depth_module.infra_profile': '424x240x15',
                    'enable_infra1': True,
                    'enable_infra2': True, 
                    'enable_depth': True, 
                    'enable_color': True,
                    'enable_sync': True,
                }]
            ),
            
            # B. Isaac ROS Visual SLAM Node
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
                    'publish_odom_to_base_tf': False, # EKF will publish this
                    'publish_map_to_odom_tf': False,  # SLAM Toolbox will publish this
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

    # 2. LD06 Lidar Node
    ld06_node = Node(
        package='ldlidar_ros2',
        executable='ldlidar_ros2_node',
        name='ld06_lidar',
        output='screen',
        parameters=[
            {'product_name': 'LDLiDAR_LD06'},
            {'laser_scan_topic_name': 'scan'},
            {'port_name': '/dev/serial/by-id/usb-Silicon_Labs_CP2104_USB_to_UART_Bridge_Controller_024TKTOY-if00-port0'},
            {'serial_baudrate': 230400},
            {'frame_id': 'ld06_lidar'}, 
            {'laser_scan_dir': True},
            {'range_min': 0.02},
            {'range_max': 12.0}
        ]
    )
    
    # Lidar PWM
    lidar_pwm_node = Node(
        package='jetsoncpp_pkg',
        executable='lidar_pwm.py',
        name='lidar_pwm_control'
    )

    # 3. Hardware MCU Node
    hw_mcu_node = Node(
        package='jetsoncpp_pkg',
        executable='hw_mcu_node',
        name='hw_mcu_node',
        output='screen',
        parameters=[{
            'serial_port': '/dev/serial/by-id/usb-Silicon_Labs_CP2104_USB_to_UART_Bridge_Controller_02CZJZRS-if00-port0', 
            'serial_baud_rate': 921600,
            'enable_tf_broadcast': False # Let EKF handle TF
        }]
    )

    # 4. Robot Localization (EKF)
    ekf_config_path = os.path.join(pkg_jetson_cpp, 'config', 'ekf.yaml')
    ekf_node = Node(
        package='robot_localization',
        executable='ekf_node',
        name='ekf_filter_node',
        output='screen',
        parameters=[ekf_config_path]
    )

    # 5. SLAM Toolbox
    slam_toolbox_node = Node(
        package='slam_toolbox',
        executable='async_slam_toolbox_node',
        name='slam_toolbox',
        output='screen',
        parameters=[{
            'use_sim_time': False,
            'odom_frame': 'odom',
            'base_frame': 'base_link',
            'map_frame': 'map',
            'scan_topic': '/scan',
            'mode': 'mapping',
            'resolution': 0.05,
            'max_laser_range': 12.0,
            'minimum_time_interval': 0.1,
            'transform_timeout': 0.2,
            'tf_buffer_duration': 30.0,
            'stack_size_to_use': 40000000,
        }]
    )

    return LaunchDescription([
        robot_state_publisher_node,
        container,
        ld06_node,
        lidar_pwm_node,
        hw_mcu_node,
        ekf_node,
        slam_toolbox_node
    ])
