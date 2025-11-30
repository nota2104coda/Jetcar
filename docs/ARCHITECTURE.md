# Architecture Overview

## Purpose
High-level: autonomous navigation to objects (e.g. red football) using RGB-D SLAM and height-aware obstacle avoidance.

## Devices
- Jetson Orin Nano: mapping, perception, planning, web server
- Raspberry Pi Pico W: motor control, encoders
- Arduino Nano: sonars, VL53L5CX, MPU6050
- Sensors: Realsense D435 (front), rplidar, LD2450, VL53L5CX, CSI rear camera, SR04 sonars, MPU6050

## Packages and responsibilities
- hw_pico_node: drivers and serial/micro-ROS bridges
- hw_realsense_node: ingest reallsense D435 data
- perception_node: object detection, people tracking
- mapping_node: RTAB-Map (RGB-D SLAM)
- navigation_node: Nav2 with voxel_layer & 3D obstacle integration
- motion_control: translate cmd_vel -> motor commands
- hmi_node: REST & streaming endpoints
- safety: watchdog & emergency stop

## Topics / Actions
- /tf, /tf_static
- /scan, /pointcloud_fused, /camera/front/depth/image_raw
- /cmd_vel (geometry_msgs/Twist)
- Action: NavigateToObject (custom) -> returns status & final pose

## Communications
- Preferred: micro-ROS for Pico + Arduino where feasible
- Fallback: robust serial protocol + serial_bridge node

## Build & test
- Provide ROS2 workspace under ros2_ws/
- Unit tests + simulation (Ignition/Gazebo)

## repo layout
PicoWCar/
├─ ARCHITECTURE.md
├─ README.md
├─ build/
├─ docker/
├─ install/
├─ libs/
│  ├─ lib-arduino/    #common libs/header files for arduino ID
├─ logs/
├─ scripts/
├─ src/
│  ├─ ardpicoW/   # arduino code for RPi Pico W
│  ├─ arduino-nano/   #arduino code for Arduino Nano
│  ├─ pi5_pkg/
│  │  ├─ launch/
│  │  ├─ msg/
│  │  ├─ pi5_pkg/
│  │  │  ├─ hw_pico_node.py
│  │  │  ├─ hw_realsense_node.py
│  │  │  ├─ mapping_node.py
│  │  │  ├─ navigation_node.py
│  │  │  ├─ perception_node.py
│  │  │  ├─ motion_control_node.py
│  │  │  └─ hmi_node.py
│  │  ├─ resource/
│  │  ├─ urdf/
├─ firmware/
│  ├─ pico_motor_control/   # micro-ROS or serial firmware
│  └─ arduino_sensor_fw/
├─ docs/
│  └─ wiring_diagram.png
└─ ci/   # tests, containers

