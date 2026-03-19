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

    # 1. Isaac ROS Container (Starting with ONLY Realsense)
    container = ComposableNodeContainer(
        name='isaac_ros_container',
        namespace='',
        package='rclcpp_components',
        executable='component_container_mt',
        output='screen',
        composable_node_descriptions=[
            # A. RealSense Camera Node (Starts immediately)
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
                    'rgb_camera.color_qos': 'SENSOR_DATA',
                    'depth_module.depth_qos': 'SENSOR_DATA',
                    'enable_infra1': True,
                    'enable_infra2': True, 
                    'enable_depth': True, 
                    'enable_color': True,
                    'enable_sync': False,
                    'frames_queue_size': 2,
                }]
            ),
        ]
    )

    # 2. Delayed Load: Isaac ROS Visual SLAM
    # We load this node into the existing container after a 5-second delay
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

    delayed_vslam = TimerAction(
        period=5.0,
        actions=[
            LogInfo(msg='[Sequential Launch] 5 seconds elapsed. Loading Visual SLAM...'),
            load_vslam
        ]
    )

    # 3. LD06 Lidar Node
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
            {'range_min': 0.02},
            {'range_max': 12.0}
        ]
    )
    
    lidar_pwm_node = Node(
        package='jetsoncpp_pkg',
        executable='lidar_pwm.py',
        name='lidar_pwm_control'
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
            'enable_tf_broadcast': False
        }]
    )

    # 5. EKF and SLAM Toolbox (Could also be delayed if needed)
    ekf_config_path = os.path.join(pkg_jetson_cpp, 'config', 'ekf.yaml')
    ekf_node = Node(
        package='robot_localization',
        executable='ekf_node',
        name='ekf_filter_node',
        output='screen',
        parameters=[ekf_config_path]
    )

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
        }]
    )

    # 6. HMI Node and Foxglove Bridge
    hmi_node = Node(
        package='jetsoncpp_pkg',
        executable='hmi_node_nobridge',
        name='hmi_node',
        output='screen'
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
        ld06_node,
        lidar_pwm_node,
        hw_mcu_node,
        ekf_node,
        slam_toolbox_node,
        hmi_node,
        foxglove_bridge_node
    ])
