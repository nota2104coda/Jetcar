import os
from launch.substitutions import Command
from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import IncludeLaunchDescription, RegisterEventHandler
from launch.event_handlers import OnProcessExit
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch_ros.actions import Node

def generate_launch_description():
    pkg_ros_gz_sim = get_package_share_directory('ros_gz_sim')
    pkg_jetcar_sim = get_package_share_directory('jetcar_sim')
    pkg_description = get_package_share_directory('jetcar_description')

    xacro_path = os.path.join(pkg_description, 'urdf', 'jetcar.urdf.xacro')
    robot_description_content = Command(['xacro ', xacro_path, ' sim_mode:=true'])

    world_path = os.path.join(pkg_jetcar_sim, 'worlds', 'room_with_obstacles.sdf')

    # 1. Gazebo Harmonic
    gz_sim = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(pkg_ros_gz_sim, 'launch', 'gz_sim.launch.py')
        ),
        launch_arguments={'gz_args': ['-r ', world_path]}.items(),
    )

    # 2. Robot State Publisher
    robot_state_publisher = Node(
        package='robot_state_publisher',
        executable='robot_state_publisher',
        output='screen',
        parameters=[{
            'robot_description': robot_description_content,
            'use_sim_time': True
        }]
    )

    # 3. Spawn Robot
    spawn_entity = Node(
        package='ros_gz_sim',
        executable='create',
        arguments=['-topic', 'robot_description', '-name', 'jetcar', '-z', '0.1'],
        output='screen'
    )

    # 4. Standard Bridge (Lidar, IMU, Odom, Clock, Teleop)
    bridge = Node(
        package='ros_gz_bridge',
        executable='parameter_bridge',
        arguments=[
            '/model/jetcar/scan@sensor_msgs/msg/LaserScan[gz.msgs.LaserScan',
            '/model/jetcar/imu@sensor_msgs/msg/Imu[gz.msgs.IMU',
            '/clock@rosgraph_msgs/msg/Clock[gz.msgs.Clock',
            '/model/jetcar/odometry@nav_msgs/msg/Odometry[gz.msgs.Odometry',
            '/model/jetcar/tf@tf2_msgs/msg/TFMessage[gz.msgs.Pose_V',
            '/model/jetcar/cmd_vel@geometry_msgs/msg/Twist[gz.msgs.Twist',
            '/model/jetcar/camera/camera_info@sensor_msgs/msg/CameraInfo[gz.msgs.CameraInfo'
        ],
        remappings=[
            ('/model/jetcar/cmd_vel', '/cmd_vel_manual'),
            ('/model/jetcar/scan', '/scan'),
            ('/model/jetcar/imu', '/imu'),
            ('/model/jetcar/odometry', '/odom'),
            ('/model/jetcar/camera/camera_info', '/camera/camera/color/camera_info')
        ],
        output='screen'
    )
    
    # 5. Camera Image Bridge
    camera_bridge = Node(
        package='ros_gz_image',
        executable='image_bridge',
        arguments=['/model/jetcar/camera/image', '/model/jetcar/camera/depth_image'],
        remappings=[
            ('/model/jetcar/camera/image', '/camera/camera/color/image_raw'),
            ('/model/jetcar/camera/depth_image', '/camera/camera/depth/image_rect_raw')
        ],
        output='screen'
    )
    
    # 6. Simulation MCU Node (Sim-specific version)
    sim_mcu_node = Node(
        package='jetcar_sim',
        executable='sim_mcu_node',
        output='screen',
        parameters=[{
            'use_sim_time': True,
            'stop_button_state': False
        }]
    )

    # 7. HMI Node (Common)
    hmi_node = Node(
        package='jetcar_common',
        executable='hmi_node',
        name='hmi_node',
        output='screen'
    )

    # 8. Controller Spawners
    joint_state_broadcaster_spawner = Node(
        package="controller_manager",
        executable="spawner",
        arguments=["joint_state_broadcaster"],
    )

    effort_controller_spawner = Node(
        package="controller_manager",
        executable="spawner",
        arguments=["effort_controller"],
    )

    spawn_controllers = RegisterEventHandler(
        event_handler=OnProcessExit(
            target_action=spawn_entity,
            on_exit=[joint_state_broadcaster_spawner, effort_controller_spawner]
        )
    )

    return LaunchDescription([
        gz_sim,
        robot_state_publisher,
        spawn_entity,
        bridge,
        camera_bridge,
        sim_mcu_node,
        hmi_node,
        spawn_controllers
    ])
