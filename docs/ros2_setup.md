# Setting Up ROS2 Minimal Environment

## Overview
This guide provides step-by-step instructions to set up a minimal ROS2 environment on both WSL2 (for development) and Raspberry Pi 5 (for deployment).

---

## Prerequisites

### 1. **WSL2**
- Ensure WSL2 is installed and configured on your system.
- Install Ubuntu 22.04 (recommended).

### 2. **Raspberry Pi 5**
- Install Raspberry Pi OS (64-bit, based on Debian).
- Ensure the Pi5 is connected to the internet.

---

## Step 1: Install ROS2 on WSL2

1. **Set Locale**:
   ```bash
   sudo locale-gen en_US en_US.UTF-8
   sudo update-locale LC_ALL=en_US.UTF-8 LANG=en_US.UTF-8
   export LANG=en_US.UTF-8
   ```

2. **Add ROS2 Repository**:
   ```bash
   sudo apt update && sudo apt install -y software-properties-common
   sudo add-apt-repository universe
   sudo apt update && sudo apt install -y curl gnupg2 lsb-release
   curl -sSL https://raw.githubusercontent.com/ros/rosdistro/master/ros.key | sudo apt-key add -
   sudo sh -c 'echo "deb http://packages.ros.org/ros2/ubuntu $(lsb_release -cs) main" > /etc/apt/sources.list.d/ros2-latest.list'
   ```

3. **Install ROS2 Packages**:
   ```bash
   sudo apt update
   sudo apt install -y ros-humble-desktop
   ```

4. **Source ROS2 Setup**:
   ```bash
   echo "source /opt/ros/humble/setup.bash" >> ~/.bashrc
   source ~/.bashrc
   ```

5. **Install Colcon** (Build Tool):
   ```bash
   sudo apt install -y python3-colcon-common-extensions
   ```

---

## Step 2: Install ROS2 on Raspberry Pi 5

1. **Set Locale**:
   ```bash
   sudo locale-gen en_US en_US.UTF-8
   sudo update-locale LC_ALL=en_US.UTF-8 LANG=en_US.UTF-8
   export LANG=en_US.UTF-8
   ```

2. **Add ROS2 Repository**:
   ```bash
   sudo apt update && sudo apt install -y software-properties-common
   sudo add-apt-repository universe
   sudo apt update && sudo apt install -y curl gnupg2 lsb-release
   curl -sSL https://raw.githubusercontent.com/ros/rosdistro/master/ros.key | sudo apt-key add -
   sudo sh -c 'echo "deb http://packages.ros.org/ros2/ubuntu $(lsb_release -cs) main" > /etc/apt/sources.list.d/ros2-latest.list'
   ```

3. **Install ROS2 Packages**:
   ```bash
   sudo apt update
   sudo apt install -y ros-humble-ros-base
   ```

4. **Source ROS2 Setup**:
   ```bash
   echo "source /opt/ros/humble/setup.bash" >> ~/.bashrc
   source ~/.bashrc
   ```

5. **Install Colcon** (Build Tool):
   ```bash
   sudo apt install -y python3-colcon-common-extensions
   ```

---

## Step 3: Verify Installation

1. **Check ROS2 Version**:
   ```bash
   ros2 --version
   ```

2. **Run ROS2 Demo**:
   ```bash
   ros2 run demo_nodes_cpp talker
   ```
   In another terminal:
   ```bash
   ros2 run demo_nodes_cpp listener
   ```

---

## Next Steps
1. Set up Docker for ROS2 development and deployment.
2. Define the directory structure for the project.
3. Begin developing ROS2 nodes.