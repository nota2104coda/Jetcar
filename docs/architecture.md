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
 subgraph jetcar_nodes["jetcar_nodes (Source Code)"]
        hw_mcu_node["hw_mcu_node (MAVLink)"]
        hmi_node["hmi_node (Buttons/LCD)"]
        lidar_pwm["lidar_pwm.py"]
        sim_mcu["sim_mcu_node"]
        adaptive["adaptive_resolution_node"]
  end
 subgraph jetcar_real["jetcar_real (Real Config)"]
        hardware_launch["hardware.launch.py"]
        loc_launch["localization.launch.py"]
  end
 subgraph jetcar_sim["jetcar_sim (Sim Config)"]
        gazebo_launch["gazebo.launch.py"]
        pc_gazebo_launch["pc_gazebo.launch.py (PC side)"]
        sim_params["sim_controllers.yaml"]
  end
 subgraph jetcar_nav["jetcar_nav (Navigation)"]
        Navigation["nav2_nodes (Planner, Controller, BT)"]
        nav_params["nav2_params.yaml"]
  end
 subgraph jetcar_description["jetcar_description (Body)"]
        RSP["robot_state_publisher (Xacro)"]
  end
 subgraph jetcar_bringup["jetcar_bringup (Integration)"]
        RealLaunch["real_robot.launch.py"]
        SimLaunch["sim_robot.launch.py"]
        JetsonSimLaunch["jetson_sim_nav.launch.py (Jetson side)"]
  end
 subgraph IsaacROS["Isaac ROS (Vision Stack)"]
        SLAM["vslam_node"]
        NvBlox["nvblox_node"]
  end

 subgraph MCU["MCU (Pico W / ESP32S3-Pico)"]
        direction TB
        Core0["Core0: Motion, MAVLink, Motors"]
        Core1["Core1: Sonar, Cliff"]
  end

    hardware_launch -- Starts --> hw_mcu_node
    hardware_launch -- Starts --> hmi_node
    gazebo_launch -- Starts --> sim_mcu
    
    SLAM --> Navigation
    Navigation --Twist--> hw_mcu_node
    hw_mcu_node -- [motor cmds]--> MCU
    hmi_node -- Commands --> Navigation
    MCU --[sensor data]--> hw_mcu_node
    RSP -- TF Tree --> SLAM
    RSP -- TF Tree --> Navigation
```

# Node + Topic Graph (Partial)
Refer to code for full details.

# Navigation Flow
Refer to code for full details.

# Roomba circuit
```mermaid
---
config:
  layout: dagre
---
graph LR
    subgraph Power_Source ["Roomba Internal Chassis"]
        BATT[Roomba Battery<br/>14.4V Nominal]
    end

    subgraph Protection_Stage ["Master Control & Protection"]
        SW_MAIN[Main Power Switch<br/>SPST 15A Rated]
        FUSE_MAIN[Main In-Line Fuse<br/>10A Blade Fuse]
    end

    subgraph Stage1_Regulator ["Primary 12V Power Bus"]
        REG_72W[72W Voltage Converter<br/>14.4V to 12V @ 6A]
        FUSE_JETSON[Sub-Fuse 6A]
        JETSON[Jetson Orin Nano<br/>12V Barrel Jack / Header]
    end

    subgraph Stage2_Regulator ["5V Power Bus"]
        FUSE_5V[Sub-Fuse 3A]
        REG_5V[Low-Voltage Converter<br/>12V to 5V Step-Down]
        RAIL_5V[5V Breadboard Rail]
        PICO_5V[ESP32-S3-Pico<br/>VBUS / 5V Pin]
    end

    subgraph Stage3_Regulator ["3.3V Power Bus"]
        PICO_3V3[ESP32-S3-Pico<br/>Internal 3.3V LDO Out]
        RAIL_3V3[3.3V Breadboard Rail]
    end

    subgraph Ground_Network ["Common Ground Bus"]
        GND[0V Common Ground Bar]
    end

    %% Positive Power Path
    BATT -->|14.4V Pos| SW_MAIN
    SW_MAIN --> FUSE_MAIN
    FUSE_MAIN -->|14.4V Unregulated| REG_72W
    
    REG_72W -->|12V Output Rail| FUSE_JETSON
    REG_72W -->|12V Output Rail| FUSE_5V
    
    FUSE_JETSON -->|12V Regulated| JETSON
    FUSE_5V -->|12V Input| REG_5V
    
    REG_5V -->|5V Output Rail| RAIL_5V
    REG_5V -->|5V Output Rail| PICO_5V
    
    PICO_5V --> PICO_3V3
    PICO_3V3 -->|3.3V Output Rail| RAIL_3V3

    %% Ground Connections
    BATT --- GND
    REG_72W --- GND
    JETSON --- GND
    REG_5V --- GND
    RAIL_5V --- GND
    PICO_5V --- GND
    RAIL_3V3 --- GND

    style Protection_Stage fill:#ffe6e6,stroke:#cc0000,stroke-width:1px
    style Stage1_Regulator fill:#e1f5fe,stroke:#0288d1,stroke-width:1px
    style Stage2_Regulator fill:#e8f5e9,stroke:#388e3c,stroke-width:1px
    style Stage3_Regulator fill:#fff3e0,stroke:#f57c00,stroke-width:1px
```    
