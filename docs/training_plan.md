Training Plan: Autonomous Systems Safety & Validation: Presenting a structured training plan for SOTIF safety analysis and automated ROS2 testing.
Phase 1: SOA Decomposition & Safety Analysis (The "Architect" Piece)
  Goal: Map the robot as a set of services and define their failure modes.

   * Task 1.1: Service Mapping. Create a "Service Map" (Mermaid diagram) of the Jetcar. 
       * Specifics: Define the Vision Service (YOLO), Localization Service (SLAM), Planning Service (Nav2), and Safety Service (Pico).
   * Task 1.2: SOTIF Failure Mode Analysis (FMEA) for SOA. 
       * Specifics: Instead of just "the robot hits a box," analyze Service Failures:
           * Scenario A: The Vision Service has high latency (too many objects). Does the Planning Service receive stale data? 
           * Scenario B: The Lidar Service drops packets (DDS issue). How does the system react?
   * Task 1.3: Interface Definition (IDL). Document the "Contracts" between services.
       * Specifics: If the cmd_vel (Motor Command) service expects 20Hz, what happens if it only gets 5Hz?

  Phase 2: Building the Automated "Integration Test" Pipeline
  Goal: Use Python to verify the "Service Mesh" (the ROS 2 network) behaves correctly.

   * Task 2.1: Headless Simulation Orchestration.
       * Specifics: Create a Python script using launch_testing that brings up the 3 core "Service Groups": Environment (Gazebo), Brain (Nav2/SLAM), and Interface (Foxglove).
   * Task 2.2: The "Oracle" Node.
       * Specifics: Build a custom Python node (The "Test Oracle") that subscribes to multiple service outputs and calculates System Health. 
       * Metric: "Is the distance between odom and goal decreasing at a rate consistent with max_velocity?"
   * Task 2.3: Automated Regression Suite.
       * Specifics: Create a folder tests/scenarios/. Write 3 .yaml files defining different room layouts. Your pipeline must loop through these, run the sim, and log results.

  Phase 3: Chaos Engineering & Fault Injection (The "System Test" Piece)
  Goal: Prove the system is "Fault Tolerant"—a key requirement for ISO 26262.

   * Task 3.1: Network Stress Testing.
       * Specifics: Use a script to flood the DDS network with dummy data. Measure at what point the Nav Service fails to update the local costmap.
   * Task 3.2: Service Kill-Tests.
       * Specifics: While the robot is navigating, programmatically kill the YOLO node. 
       * Success Criteria: The robot must switch to a "Lidar-only" degraded mode or perform a Safe Stop.
   * Task 3.3: Data Corruption. 
       * Specifics: Inject "Salt and Pepper" noise into the Lidar Scan service via a Python shim. Verify if the Planning Service filters it out or "sees" fake walls.

  Phase 4: Model-Based Validation (MATLAB/Simulink Integration)
  Goal: Use your MATLAB access to show "Model-in-the-Loop" (MIL) testing.

   * Task 4.1: The Safety Monitor Model. 
       * Specifics: Create a Simulink model that acts as a Runtime Monitor. It subscribes to ROS 2 topics, runs a "Safety Shield" algorithm, and publishes a "Veto" signal if the robot is about to collide.
   * Task 4.2: Co-Simulation.
       * Specifics: Run Gazebo for the physics and MATLAB for the high-level logic. This shows you can handle professional "Toolchains."

  ---

  How this relates to SOA (The "Why"):
   1. Loose Coupling: You are testing if the "Brain" can survive without the "Eyes" (Vision Service).
   2. Encapsulation: You are testing the "Safety Layer" (Pico) as a completely independent service that overrides the "Non-Realtime" services (Jetson).
   3. Observability: You are using Foxglove and Rosbags as "Distributed Tracing" tools to find bottlenecks in the service chain.

  Which specific "Service Failure" interests you most for our first test case?
   * A) A service crashing (The "Kill" test).
   * B) A service sending bad data (The "Sensor Noise" test).
   * C) A service being too slow (The "Latency" test).


  How to Start
  1. Define one Requirement (from Phase 1).
  2. Build the Test for it (from Phase 2).
  3. Repeat.

------------------------------------------------------------------

Here is a step-by-step plan to systematically diagnose why Nav2 is failing to output motor commands, why nodes might be disappearing, and how to track down the /cmd_vel_nav remapping issue. 

  You will need to open several terminal windows. Start your simulation and Nav2 stack as you normally would:
   1. Terminal 1: ros2 launch jetcar_sim gazebo.launch.py
   2. Terminal 2: ros2 launch jetcar_bringup sim_robot.launch.py

  Once everything is seemingly running, proceed through these steps in order.

  Step 1: Verify the Output Topic (cmd_vel_nav)
  First, let's see if Nav2 is actually generating commands and if they are going to the right place.

   1. List all topics:
   1     ros2 topic list -t
      Look for /cmd_vel, /cmd_vel_nav, and /cmd_vel_smoothed. Note their types (usually geometry_msgs/msg/Twist).

   2. Check who is talking and listening on the target topic:
   1     ros2 topic info /cmd_vel_nav --verbose
      This is crucial. You should see controller_server or nav2_smoother as a publisher, and your sim_mcu_node (or whatever drives the wheels) as a subscriber. If the publisher count is 0, Nav2 isn't outputting.
  If the subscriber count is 0, your motor controller isn't listening to the remapped topic.

   3. Echo the topic while sending a goal:
      Send a goal via Foxglove. In a terminal, run:
   1     ros2 topic echo /cmd_vel_nav
      If it prints data, the problem is your motor node. If it prints nothing, the problem is inside Nav2.

  Step 2: Check Node Lifecycles (Why nodes "go missing")
  Nav2 uses a strict state machine. If nodes encounter an error (like missing TF or sensor data), the lifecycle_manager might downgrade them from active to inactive, making them appear unresponsive.

   1. Check the state of the controller server:
   1     ros2 lifecycle get /controller_server
      If it says unconfigured or inactive, Nav2 has halted. It must say active to publish velocity commands.

   2. Check all Nav2 nodes:
   1     ros2 node list | grep server
      Verify planner_server, controller_server, behavior_server, and bt_navigator are all present.

  Step 3: Investigate Node Timeouts and Crashes
  If nodes are actively disappearing from ros2 node list over time, they are crashing. We need to find out why.

   1. Check the Nav2 Bringup logs directly:
      Look at Terminal 2 (where you launched sim_robot.launch.py). Scroll up. You are looking for:
       * Red text (Errors): Especially lines mentioning bond broken, timer callback exceptions, or segmentation faults.
       * Yellow text (Warnings): Look for warnings about "Extrapolation into the past" or "Transform timeout."

   2. Use ros2 doctor to find networking/QoS drops:
      Open a new terminal and run:
   1     ros2 doctor --report
      This will tell you if topics are dropping messages or if there are Quality of Service (QoS) mismatches (e.g., Nav2 publishing "Reliable" but your node subscribing "Best Effort").

  Step 4: Verify the Transform (TF) Tree
  Nav2 requires a perfect, unbroken chain of transforms from the map down to the wheels. If this breaks, Nav2 stops outputting cmd_vel immediately.

   1. Check the critical path:
   1     ros2 run tf2_ros tf2_echo map base_link
      If this errors out saying "Exception thrown," your localization (SLAM or AMCL) or your odometry (EKF) has failed.

   2. Monitor transform rates:
   1     ros2 run tf2_ros tf2_monitor
      Look at the "Net delay" and "Frames." If the delay between odom and base_link gets too high, Nav2 will timeout and nodes may deactivate.

  How to proceed:
  Run through these four steps. 
   * If Step 1 fails (no publisher), move to Step 2.
   * If Step 2 reveals nodes are inactive, the logs in Step 3 or the TF tree in Step 4 will usually tell you why they deactivated.

  Once you identify the specific failure (e.g., "The controller_server is crashing due to a TF timeout after 30 seconds"), let me know. We can fix it, and then immediately write a launch_testing script to
  automatically check that specific TF rate on every future run.

