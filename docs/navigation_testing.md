# Jetcar Navigation & Simulation Testing Guide

This guide provides the necessary steps to verify the ROS 2 navigation stack, simulation bridge, and Foxglove integration.

## 1. Launch Sequence

### PC (Simulation Host)
```bash
# 1. Start Gazebo and the ROS bridge
ros2 launch jetcar_sim gazebo.launch.py
```

### Jetson (Navigation Host)
```bash
# 2. Start SLAM, Nav2, and the vision stack
ros2 launch jetcar_bringup sim_robot.launch.py
```

---

## 2. Foxglove Studio Setup

### 3D Panel Topics to Enable
To see the robot and its intent, enable these topics in the **3D Panel Settings**:
- **`/tf`**: Visualizes the robot's frames.
- **`/scan`**: Visualizes the lidar points.
- **`/plan`**: Shows the global path (Nav2's chosen route).
- **`/local_costmap/costmap`**: Shows the local avoidance zone.
- **`/map`**: Shows the SLAM-generated floor plan.

### Sending Goals
Foxglove publishes to `/move_base_simple/goal`. The `goal_fixer` node relays this to Nav2's `/goal_pose`.
- **Tool:** Use the "2D Nav Goal" button in Foxglove.
- **Troubleshooting:** If the robot doesn't move, ensure **"Auto Mode"** is toggled ON in the HMI panel.

---

## 3. Diagnostic Commands

### Check the Transform Tree
If SLAM or Navigation fails, verify the "Map -> Odom -> Base" chain:
```bash
ros2 run tf2_ros tf2_echo map base_link
```

### Verify Sensor Flow
Check if data is reaching the Jetson from the PC:
```bash
# Lidar
ros2 topic hz /scan
# Simulation Clock
ros2 topic hz /clock
# Ground Truth Odom
ros2 topic hz /mcu/odom
```

### Audit Connections
Identify topics without publishers or subscribers:
```bash
ros2 doctor
```

---

## 4. Known Fixes Applied
- **Frame ID:** SLAM and Nav2 are configured to use `base_link` (not `base_footprint`).
- **Goal Timestamps:** The `goal_fixer` node automatically repairs `Time=0` timestamps from Foxglove to prevent "Extrapolation into the past" errors.
- **Bridge Remapping:** Gazebo's internal topics (`/model/jetcar/tf`, etc.) are remapped to standard ROS topics via `gazebo.launch.py`.
- **Tolerances:** Transform tolerances are set to **1.0s** for simulation and **0.5s** for hardware to handle network/CPU jitter.
