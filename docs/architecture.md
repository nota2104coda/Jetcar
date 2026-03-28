# System Architecture Diagram

# Budgets
**RAM for Jetson Orin Nano 8GB**
Current state: 1.6GB with Isaaac, Ld06, Realsense,gnome display, MCU bridge and foxglove bridge all running. Isaac 
Need to manage Yolo/other recognition framework and an llm/VLA model in remaining space. Keep 0.8GB spare at all times. 

estimates from chatgpt
Realsense
depth resolution: 640×480
fps: 15
decimation filter ON
pointcloud OFF
Saves 200–300 MB.

RTAB-Map
Mem/IncrementalMemory = true
Kp/MaxFeatures = 400
RGBD/OptimizeFromGraphEnd = true
RGBD/LinearUpdate = 0.1
Reg/Strategy = 0 (visual odom only)
RGBD/MaxRange = 4.0
Grid/FromDepth = false (use external costmap)
Saves ~1 GB.

Nav2
reduce global costmap resolution to 0.1 or 0.12 m
Voxel layer:
max_z = robot_height + 10 cm
z_voxels = 8–12
Use only local 3D info if possible
Saves ~150 MB.

⭐ Recommended “safe mode” loadout
If you want reliability and smooth operation:

Run these simultaneously:
Realsense depth 640×480 @ 15 fps
RTAB-Map tuned
Nav2 costmap (voxel layer on)
RPLidar
Foxglove bridge


Avoid:
Running YOLO inference constantly on CPU
Storing large pointcloud histories
Uncompressed rear camera streams
This mode runs in 3.8–4.5 GB comfortably.
**Timing for Jetson Orin Nano**
**timing for ESP32S3-Pico**
Core0
Core1

```mermaid
graph TD
    PC[PC Ubuntu 22.04] <-- WiFi for Gazebo sim--> Jetson[Jetson Orin Nano]
    Jetson -- USB-UART(CP2104 USB-UART1 of MCU) --> MCU[Pico W/ESP32S3-Pico]
    Realsense[Realsense D435 binocular camera] -- USB#1 --> Jetson
    MCU -- I2C0: 4x wheel torque --> PCA9685_AWDDriver[4 Wheels]
    PCA9685_AWDDriver[4 Wheels] -- Hall Encoder Pins --> MCU
    Jetson -- Foxglove Bridge Server --> Web[Foxglove in Web Browser]
    Web[Foxglove in Web Browser] --User commands--> Jetson
    Radar[LD2450 Radar] -- UART0 --> MCU
    Lidar2D[LD06 2D Lidar] --USB-UART(CP2104 to UART0 of Jetson) --> Jetson
    Lidar[VL53L5X front 8x8 Lidar] -- I2C0 --> MCU
    IMU[IMU-MPU6050] -- I2C0 --> MCU
    USensors[HC SR04 Ultrasonic Sensors] -- voltage level shifter --> MCU
    IRSensors[IR Cliff Sensors] -- voltage level shifter --> MCU
```

# Software Architecture Diagram

```mermaid
---
config:
  layout: dagre
---
flowchart TB
 subgraph ROS2Nodes["ROS 2 Nodes on Jetson"]
        Vision["vision_node::Camera, ld06_lidar"]
        Recognition["yolo_node"]
        LLM["llm_node"]
        SLAM["vslam_node"]
        Navigation["nav2_node::Path Planning"]
        Control["control_node"]
        hw_mcu_node["hw_mcu_node CP2104 mavlink"]
        hmi_node["Foxglove bridge"]
  end
 subgraph Core0["Core0"]
        MotionArb["Motion arbitration"]
        mcu_mavlink["comms with Jetson"]
        MotorDriver["motorDriver"]
        LD2450["uart LD2450"]
        I2CSensors["Encoders, MPU6050, VL53L5X"]
  end
 subgraph Core1["Core1"]
        Sonar["Sonar"]
        CliffSensor["Cliffsensor"]
  end
 subgraph MCU["Pico W / ESP32S3-Pico"]
        Core0
        Core1
  end
    Vision -- Sensor Data --> SLAM
    SLAM -->Navigation
    Navigation --Twist--> Control
    Control -- [motor commands[] --> hw_mcu_node
    hw_mcu_node -- Sensor Data --> Control
    hw_mcu_node -- [motor cmds]--> mcu_mavlink
    hmi_node -- Commands --> Control
    Vision -- Visualization --> hmi_node
    Sonar --> MotionArb
    CliffSensor --> MotionArb
    LD2450 --> MotionArb
    mcu_mavlink --[sensor data]--> hw_mcu_node
    mcu_mavlink --> MotionArb
    MotionArb --> MotorDriver
    I2CSensors --> mcu_mavlink
    LD2450 --> mcu_mavlink
```

# Node + Topic Graph

```mermaid
---
config:
  layout: dagre
---
graph LR
  classDef topic fill:#f5f5f5,stroke:#808080,stroke-width:1px,color:#000,font-size:11px;

  subgraph UI
    Foxglove[foxglove_bridge]
    HMI[hmi_node]
  end

  subgraph Sensors
    Camera[realsense2_camera(camera)]
    LD06[ld06_lidar]
    PWM[lidar_pwm_control]
    MCU[hw_mcu_node]
  end

  subgraph "Isaac ROS Container"
    VSLAM[visual_slam_node]
    Converter[image_format_converter_node]
    NvBlox[nvblox_node]
  end

  subgraph Localization
    EKF[ekf_filter_node]
    SlamTB[slam_toolbox]
    RSP[robot_state_publisher]
  end

  subgraph "Nav2 Stack"
    Planner[planner_server]
    Controller[controller_server]
    PathSmoother[smoother_server]
    VelSmooth[velocity_smoother]
    Behaviors[behavior_server]
    BTN[bt_navigator]
    Waypoint[waypoint_follower]
    Lifecycle[lifecycle_manager_navigation]
  end

  topicButtons(["/hmi/button_states"])
  topicGoal(["/goal_pose::geometry_msgs/PoseStamped"])
  topicInfra(("Infra stereo + info\n/camera/camera/infra{1,2}/image_rect_raw"))
  topicIMU(["/mcu/imu::sensor_msgs/Imu"])
  topicDepth(("Depth image + info\n/camera/camera/depth/image_rect_raw"))
  topicColorRaw(["/camera/camera/color/image_raw"])
  topicColorRGB(["/camera/camera/color/image_rgb"])
  topicPose(["/visual_slam/tracking/vo_pose"])
  topicOdom(["/visual_slam/tracking/odometry"])
  topicWheelOdom(["/mcu/odom::nav_msgs/Odometry"])
  topicScan(["/scan::sensor_msgs/LaserScan"])
  topicCloud(["/pointcloud2d::sensor_msgs/PointCloud2"])
  topicEKF(["/odometry/filtered::nav_msgs/Odometry"])
  topicMap(["/map::nav_msgs/OccupancyGrid"])
  topicCostLocal(("nvblox ESDF + costmaps"))
  topicTF(("TF map→odom→base_link"))
  topicCmdVel(["/cmd_vel::geometry_msgs/Twist"])

  Foxglove --> topicGoal
  HMI --> topicGoal
  topicGoal --> BTN
  HMI --> topicButtons --> Foxglove

  Camera --> topicInfra --> VSLAM
  MCU --> topicIMU --> VSLAM
  Camera --> topicDepth --> NvBlox
  Camera --> topicColorRaw --> Converter --> topicColorRGB --> NvBlox
  VSLAM --> topicPose
  topicPose --> NvBlox
  topicPose --> EKF
  VSLAM --> topicOdom --> EKF
  MCU --> topicWheelOdom --> EKF
  EKF --> topicEKF --> Planner
  topicEKF --> Controller
  NvBlox --> topicCostLocal --> Planner
  topicCostLocal --> Controller
  LD06 --> topicScan --> SlamTB
  topicScan --> Controller
  SlamTB --> topicMap --> Planner
  LD06 --> topicCloud --> Foxglove
  RSP --> topicTF
  topicTF --> Planner
  topicTF --> Controller
  topicTF --> Foxglove

  BTN --> Planner
  BTN --> Behaviors
  BTN --> Waypoint
  Planner --> PathSmoother --> Controller --> VelSmooth --> topicCmdVel --> MCU
  Lifecycle -.-> Planner
  Lifecycle -.-> Controller
  Lifecycle -.-> PathSmoother
  Lifecycle -.-> VelSmooth
  Lifecycle -.-> Behaviors
  Lifecycle -.-> BTN
  Lifecycle -.-> Waypoint
  PWM --> LD06

  class topicButtons,topicGoal,topicInfra,topicIMU,topicDepth,topicColorRaw,topicColorRGB,topicPose,topicOdom,topicWheelOdom,topicScan,topicCloud,topicEKF,topicMap,topicCostLocal,topicTF,topicCmdVel topic;
```

# Navigation 

```mermaid
---
config:
  layout: dagre
---
graph LR
  classDef topic fill:#f5f5f5,stroke:#666,color:#111,font-size:12px;

  RS[realsense2_camera]
  Converter[image_format_converter_node]
  VSLAM[visual_slam_node]
  NvBlox[nvblox_node]
  LD06[ld06_lidar]
  SlamTB[slam_toolbox]
  EKF[ekf_filter_node]
  Nav2G[Nav2 global_costmap]
  Nav2L[Nav2 local_costmap]
  Planner[planner_server]
  PathSmoother[smoother_server]
  Controller[controller_server]
  VelSmooth[velocity_smoother]
  Behaviors[behavior_server]
  BTN[bt_navigator]
  Waypoint[waypoint_follower]
  Lifecycle[lifecycle_manager_navigation]
  MCU[hw_mcu_node]
  RSP[robot_state_publisher]
  Foxglove[foxglove_bridge]
  HMI[hmi_node]

  tInfra(("IR stereo + info"))
  tIMU(("/mcu/imu"))
  tDepth(("Depth image + info"))
  tColorRaw(("Color image (raw)"))
  tColorRGB(("Color image (rgb8)"))
  tPose(("/visual_slam/tracking/vo_pose"))
  tOdom(("/visual_slam/tracking/odometry"))
  tWheelOdom(("/mcu/odom"))
  tFiltered(("/odometry/filtered"))
  tScan(("/scan"))
  tMap(("/map"))
  tESDF(("nvblox ESDF + meshes"))
  tGoalPose(("NavigateToPose / FollowWaypoints"))
  tCmdVelAuto(("/cmd_vel_auto"))
  tCmdVelManual(("/cmd_vel_manual"))
  tTF(("TF map→odom→base_link"))

  RS --> tInfra --> VSLAM
  MCU --> tIMU --> VSLAM
  RS --> tDepth --> NvBlox
  RS --> tColorRaw --> Converter --> tColorRGB --> NvBlox
  VSLAM --> tPose --> NvBlox
  VSLAM --> tOdom --> EKF
  MCU --> tWheelOdom --> EKF
  EKF --> tFiltered
  tFiltered --> Nav2G
  tFiltered --> Nav2L
  tFiltered --> Planner
  tFiltered --> Controller

  LD06 --> tScan --> SlamTB
  tScan --> Nav2L
  SlamTB --> tMap --> Nav2G

  NvBlox --> tESDF --> Nav2L
  tESDF --> Nav2G

  Foxglove --> tGoalPose
  HMI --> tGoalPose
  tGoalPose --> BTN
  BTN --> Planner
  BTN --> Behaviors
  BTN --> Waypoint
  Waypoint --> BTN

  Nav2G --> Planner
  Nav2L --> Controller
  Planner --> PathSmoother --> Controller
  Controller --> VelSmooth --> tCmdVelAuto --> MCU
  foxglove_bridge --> tCmdVelManual --> MCU
  Behaviors --> Controller

  Lifecycle -.-> Nav2G
  Lifecycle -.-> Nav2L
  Lifecycle -.-> Planner
  Lifecycle -.-> PathSmoother
  Lifecycle -.-> Controller
  Lifecycle -.-> VelSmooth
  Lifecycle -.-> Behaviors
  Lifecycle -.-> BTN
  Lifecycle -.-> Waypoint

  RSP --> tTF
  tTF --> Nav2G
  tTF --> Nav2L
  tTF --> Planner
  tTF --> Controller

  class tInfra,tIMU,tDepth,tColorRaw,tColorRGB,tPose,tOdom,tWheelOdom,tFiltered,tScan,tMap,tESDF,tGoalPose,tCmdVelAuto,tTF topic;
```