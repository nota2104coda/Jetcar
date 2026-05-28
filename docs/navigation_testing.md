this is my 2nd PC while the jetson is the primary robot computer. I notive that, when i launch @src/jetcar_bringup/launch/real_robot.launch.py , the processor util on the jetson is around 55% and no GPU      
   util. WHen I run @src/jetcar_bringup/launch/sim_robot.launch.py , however, of the 6 jetson cpu cores, 5 are at 95%. Clearly something is very different between real_robot and sim_robot. And clearly, i'm      
   underutilising the GPU. Only advise me what i should do to understand and debug this                                                                                                                            
▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀

  ✓  ReadManyFiles  Attempting to read files from src/jetcar_bringup/launch/real_robot.launch.py, src/jetcar_bringup/launch/sim_robot.launch.py → Read 2 file(s)
  Debugging CPU/GPU Discrepancy: Researching configuration differences between real and simulated robot launches to identify causes for high CPU usage and lack of GPU utilization.

  ✓  ReadFile  src/.../config/nav2_params.yaml
  ✓  ReadFile  src/.../config/nav2_params_sim.yaml
  ✓  ReadFile  src/.../launch/hardware.launch.py
  ✓  ReadFile  src/.../launch/navigation.launch.py
  ✓  ReadFile  src/jetcar_common/config/ekf.yaml

✦ To understand and debug the CPU/GPU discrepancy between your real and simulated robot launches, you should follow this systematic approach. The 95% CPU usage on 5/6 cores suggests a significant processing
  bottleneck, likely related to data frequency or network overhead, while the 0% GPU usage is expected given your current launch configuration.

  1. Identify the CPU "Hogs"
  The first step is to pinpoint which specific ROS 2 nodes are consuming the cycles.
   * Action: Run htop or top on the Jetson while the simulation launch is active.
   * What to look for:
       * slam_toolbox_node: Often the biggest consumer if scan rates are high or map resolution is fine.
       * controller_server / planner_server: If the costmap update frequencies are too high.
       * ekf_node: Can spike if it's receiving high-frequency IMU/Odom data over the network.
       * RMW/DDS threads: If you see high CPU in threads not named after ROS nodes, it's likely the middleware struggling with network traffic.

  2. Profile Topic Frequencies and Bandwidth
  Simulation environments (like Gazebo or Isaac Sim) often default to higher data rates than real hardware, or they may be running "faster than real-time."
   * Action: Run ros2 topic hz on critical topics:
       * ros2 topic hz /scan (Lidar)
       * ros2 topic hz /mcu/odom and /mcu/imu (Odometry/IMU)
       * ros2 topic hz /tf (Transforms)
   * Debugging Logic: If your real Lidar is 10Hz but the simulation is 40Hz, slam_toolbox will work 4x harder. If /tf is exceeding 100-200Hz, the TF buffer overhead becomes significant.

  3. Analyze Network/DDS Overhead
  Since your simulation is on a 2nd PC, all sensor data (Scan, Odom, IMU, and potentially Images) is traveling over the network.
   * Action: Check network throughput using nload or iftop. 
   * Why this matters: Cross-machine communication in ROS 2 uses DDS. If you are using the default FastDDS without a specific configuration (like FASTRTPS_DEFAULT_PROFILES_FILE), the overhead of
     serializing/deserializing high-frequency data from another PC is much higher than local inter-process communication.

  4. Investigate the GPU Underutilization
  Your GPU utilization is 0% because your "GPU-heavy" nodes are currently disabled.
   * Observation: In both real_robot.launch.py and sim_robot.launch.py, the isaac_container, visual_slam, and nvblox nodes are commented out in the LaunchDescription return.
   * What to do: To utilize the GPU, you must uncomment these nodes. These are Isaac ROS nodes specifically designed to offload VSLAM and 3D reconstruction to the Jetson's hardware accelerators (GPU and PVA).

  5. Check for "Dead Loops" in TF/Time
   * Check the /clock topic: Run ros2 topic hz /clock. If it's extremely high or erratic, it can cause nodes to spin CPU cycles trying to synchronize.
   * Missing TFs: If robot_state_publisher is commented out in sim_robot.launch.py (as it appears to be), other nodes might be spamming error logs or retry loops looking for base_link transformations, which can
     ironically spike CPU usage.

  Recommended Debugging Commands

    1 # 1. Monitor CPU per process
    2 htop
    3
    4 # 2. Check for high-frequency topics
    5 ros2 topic hz /scan
    6 ros2 topic hz /tf
    7 ros2 topic hz /clock
    8
    9 # 3. Check for log spam (common cause of CPU spikes)
   10 ros2 launch jetcar_bringup sim_robot.launch.py | grep -i error
   11
   12 # 4. Visualize TF tree to ensure no components are "disconnected"
   13 ros2 run tf2_tools view_frames

  Next Step Recommendation: If you find that slam_toolbox or ekf are the bottlenecks, try reducing the update_frequency in nav2_params_sim.yaml or capping the simulation's output frequency on the 2nd PC. To fix
  the GPU issue, you'll need to enable the Isaac ROS components and ensure they are receiving the simulated camera streams.


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
