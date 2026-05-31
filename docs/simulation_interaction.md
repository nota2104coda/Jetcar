# Simulation Architecture Interaction

This diagram explains how `gazebo.launch.py` and `nav_sim.launch.py` interact within the Jetcar simulation environment.

```mermaid
graph TD
    subgraph "gazebo.launch.py (Body & Interface)"
        GZ[Gazebo Harmonic Engine]
        World[Room & Obstacles]
        Bridge[ROS_GZ_Bridge]
        MCU[Sim MCU Node]
        HMI[hmi_node]
        Fox[Foxglove Bridge]
    end

    subgraph "nav_sim.launch.py (Autonomous Brain)"
        NV[nvblox: 3D Mapping]
        Nav2[Nav2 Stack: Path Planning]
        Adapt[Adaptive Resolution Node]
    end

    %% Sensor Flow (Body -> Brain)
    GZ -->|Lidar/IMU/Camera| Bridge
    Bridge -->|/scan, /imu, /camera| NV
    Bridge -->|/odom| Nav2
    Bridge -->|All Topics| Fox

    %% Control Flow (Brain -> Body)
    Nav2 -->|/cmd_vel_nav| MCU
    MCU -->|Motor Effort| Bridge
    Bridge -->|Wheel Torque| GZ

    %% HMI Interaction
    Bridge -->|/cmd_vel_manual| HMI
    HMI -->|/hmi/button_states| Fox
    HMI -.->|Starts| Fox

    %% Adaptive Logic
    Adapt -.->|Adjusts Resolution| GZ
```

## Key Interactions:
1.  **Infrastructure & Interface:** `gazebo.launch.py` handles the physical world, the robot body, and the user interface. It starts the **HMI Node**, which automatically launches the **Foxglove Bridge** for real-time visualization.
2.  **Sensor Data:** The Bridge translates raw Gazebo physics into standard ROS topics (`/scan`, `/imu`, `/camera/...`). These are used by both the HMI for display and the Brain for navigation.
3.  **The "Driver" (Sim MCU):** The `sim_mcu_node` acts as the motor controller. It listens for velocity commands from either the user (Manual) or Nav2 (Auto), calculates the required wheel torque, and sends it back to Gazebo.
4.  **Autonomous Brain:** `nav_sim.launch.py` provides the intelligence. **nvblox** builds a 3D map from camera data, and **Nav2** uses that map to plan paths and send movement commands (`cmd_vel_nav`) to the MCU.
5.  **Feedback Loop:** As the rover moves, its simulated position (`odom`) is bridged back to ROS, allowing Nav2 and nvblox to update the robot's location in the virtual room.
