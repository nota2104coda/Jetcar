Training Plan: Autonomous Systems Safety & Validation: Presenting a structured training plan for SOTIF safety analysis and automated ROS2 testing.

✦ This plan is designed to transform your hobby project into a professional portfolio piece for Systems Test and Safety Engineering roles. It leverages your ROS 2 foundation and MATLAB/Simulink access while
  focusing on architectural rigor.

  Phase 1: SOTIF & Functional Safety Analysis (The "Design" Piece)
  Goal: Prove you can bridge the gap between high-level safety standards and technical requirements.

   * Task 1.1: Functional Definition. Define exactly how "Obstacle Avoidance" works in your Jetcar. List inputs (Lidar, Vision, Radar) and outputs (Motor PWM).
   * Task 1.2: Hazard Analysis & Risk Assessment (HARA). Use the docs/safety_analysis.md I started. Identify what happens when the system fails (e.g., "Robot hits a person because the radar was blocked").
   * Task 1.3: SOTIF Specific Analysis. Identify "Triggering Conditions." 
       * Example: A "transparent glass door" is not a hardware failure, but the Lidar can't see it. This is a SOTIF issue.
   * Task 1.4: Deriving Safety Requirements. Write 3-5 specific requirements (e.g., "The robot must stop within 20cm of an obstacle detected at 1.0 m/s").

  Phase 2: Building the Automated Test Harness (The "Engineering" Piece)
  Goal: Create a reusable framework that runs simulation scenarios and generates pass/fail reports.

   * Task 2.1: Setup launch_testing. This is the standard ROS 2 way to run tests that involve multiple nodes and Gazebo.
   * Task 2.2: Build a Scenario Manager (Python). Write a script that uses the Gazebo ROS API to:
       * Reset the simulation.
       * Spawn an obstacle at a specific coordinate.
       * Command the robot to move toward the obstacle.
   * Task 2.3: Create "Monitors". Write Python nodes that subscribe to /odom and /scan to measure:
       * Minimum Distance: Did the robot hit the box?
       * Time to Brake: How long between the obstacle appearing and the motors stopping?

  Phase 3: Executing Test Campaigns & Fault Injection
  Goal: Demonstrate "Negative Testing"—proving the system handles bad data gracefully.

   * Task 3.1: The Baseline Test. Run the robot toward a static box. Verify it stops or replans (Success).
   * Task 3.2: The "Blue Ball" SOTIF Test. Simulate a dynamic obstacle rolling into the path.
   * Task 3.3: Fault Injection. 
       * Scenario: Kill the Lidar node while the robot is moving. 
       * Requirement: Does the robot immediately stop, or does it keep driving blind?

  Phase 4: CI/CD & Portfolio Wrap-up
  Goal: Show that these tests run automatically on every code change.

   * Task 4.1: GitHub Actions Integration. Set up a workflow to run your launch_testing suite in a headless Docker container.
   * Task 4.2: Documentation. Create a "Safety Case" folder in your repo containing the SOTIF analysis, the test results (PDF/HTML), and a video of a successful automated test run.

  ---

  How to Start
  1. Define one Requirement (from Phase 1).
  2. Build the Test for it (from Phase 2).
  3. Repeat.

