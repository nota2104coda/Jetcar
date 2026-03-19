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

    Foxglove[foxglove_bridge]
    HMI[hmi_node]
    robot_motion_node[robot_motion_node]
    MCUNode[hw_mcu_node]
    MCU_UART[USB-UART]

    topic_cmd_vel_manual(["/cmd_vel/manual::geometry_msgs/Twist"])
    topic_stop_button(["/stop_button::std_msgs/Bool"])
    topic_auto_mode(["/auto_mode_button::std_msgs/Bool"])
    topic_button_states(["/hmi/button_states::robot_msgs/ButtonStates"])
    topic_cmd_wrench(["/cmd_wrench::geometry_msgs/Wrench"])
    topic_imu(["/mcu/imu::sensor_msgs/Imu"])
    topic_range_front(["/mcu/range/front::sensor_msgs/Range"])
    topic_range_rear(["/mcu/range/rear::sensor_msgs/Range"])
    topic_cliff_front(["/mcu/cliff/front::sensor_msgs/Range"])
    topic_cliff_rear(["/mcu/cliff/rear::sensor_msgs/Range"])
    topic_esc(["/esc_telemetry::std_msgs/Float32MultiArray"])
    topic_odom(["/odom::nav_msgs/Odometry"])

    Foxglove --> topic_cmd_vel_manual
    topic_cmd_vel_manual --> HMI
    Foxglove --> topic_stop_button
    topic_stop_button --> HMI
    Foxglove --> topic_auto_mode
    topic_auto_mode --> HMI
    HMI --> topic_button_states
    topic_button_states --> robot_motion_node
    robot_motion_node --> topic_cmd_wrench
    topic_cmd_wrench --> MCUNode

    subgraph mcuNode
    MCUNode -- SET_ACTUATOR_CONTROL_TARGET (mavlink) --> MCU_UART
    MCU_UART -- HIGHRES_IMU (mavlink) --> MCUNode
    MCU_UART -- DISTANCE_SENSOR (mavlink) --> MCUNode
    MCU_UART -- ESC_TELEMETRY_1_TO_4 (mavlink) --> MCUNode
    end
    MCUNode --> topic_imu
    topic_imu --> robot_motion_node
    MCUNode --> topic_range_front
    topic_range_front --> robot_motion_node
    MCUNode --> topic_range_rear
    topic_range_rear --> robot_motion_node
    MCUNode --> topic_cliff_front
    topic_cliff_front --> robot_motion_node
    MCUNode --> topic_cliff_rear
    topic_cliff_rear --> robot_motion_node
    MCUNode --> topic_esc
    topic_esc --> robot_motion_node

    robot_motion_node --> topic_odom
    topic_odom --> Foxglove

    class topic_cmd_vel_manual,topic_stop_button,topic_auto_mode,topic_button_states,topic_cmd_wrench topic;
    class topic_imu,topic_range_front,topic_range_rear,topic_cliff_front,topic_cliff_rear,topic_esc,topic_odom topic;
```

# Navigation 

```mermaid
---
config:
  layout: dagre
---

graph LR
    classDef topic fill:#f5f5f5,stroke:#666,color:#111,font-size:12px;

    RS[Realsense D435]
    VSLAM[visual_slam_node]
    NvBlox[nvblox_node]
    LD06[LD06 lidar node]
    SLAMTB[slam_toolbox]
    Nav2G[Nav2 global_costmap]
    Nav2L[Nav2 local_costmap]
    Planner[Nav2 planner_server]
    Controller[Nav2 controller_server]
    Robot[MCU / base_link]

    tInfra0(("visual_slam/image_0\n+ camera_info_0"))
    tInfra1(("visual_slam/image_1\n+ camera_info_1"))
    tIMU(("visual_slam/imu"))
    tOdom(("visual_slam/odom\nnav_msgs/Odometry"))
    tTF(("TF: map→odom→base"))
    tTSDF(("nvblox_node/tsdf_layer"))
    tCostLocal(("nvblox_node/costmap/local"))
    tCostGlobal(("nvblox_node/costmap/global"))
    tScan(("/scan"))
    tMap(("/map\nnav_msgs/OccupancyGrid"))
    tCmdVel(("/cmd_vel"))
    tControl(("Actuator cmds\n(mavlink/twist)"))

    RS --> tInfra0 --> VSLAM
    RS --> tInfra1 --> VSLAM
    VSLAM --> tIMU --> NvBlox
    VSLAM --> tOdom --> NvBlox
    VSLAM --> tTF --> NvBlox
    NvBlox --> tTSDF --> NvBlox
    NvBlox --> tCostLocal --> Nav2L
    NvBlox --> tCostGlobal --> Nav2G

    LD06 --> tScan --> SLAMTB
    SLAMTB --> tMap --> Nav2G
    tScan --> Nav2L

    Nav2G --> Planner
    Nav2L --> Controller
    Planner --> Controller
    Controller --> tCmdVel --> Robot
    Robot --> tControl --> Controller
```