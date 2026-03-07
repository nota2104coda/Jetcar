# Jetcar Project

## Overview
Jetcar is a 4-wheel differential drive robot system using Jetson Orin Nano and an MCU(Pi Pico W RP2040 or ESP32S3-Pico). It uses a Realsense 435 camera, LD06 lidar and other sensor data for autonomous navigation and object search, controlled via a web interface. It uses ROS2 Humble.

## Use Cases: 
when the user asks to navigate to the red football, it scans the room by spinning the body, until the object is found. It then navigates to the ball and stops at a safe distance. If the path is below a chair, it should intelligently see the height and go below or around. 

## What Works and what's next:
5-Mar-2026: Teleoperation using Foxglove GUI works. Gathering VSLAM data using ISAAC ROS and 2D lidar data using LD06 library works. 

## Learning Objectives
-Learn use of FreeRTOS or Zephyr on Pi Pico, with concurrent processes on both cores.
-Learn considerations for safe use of C++ code and aligning as close as possible to MISRA concepts. Use non-blocking, deterministic code.
-Learn to configure a robot in ROS2 Humble. 
-Learn to apply LLM/VLM for mobile robots. 
-Learn to test unit/integration/system level using test frameworks and Isaac Sim, without having to actually drive the robot

## Hardware Architecture
**Jetson Orin Nano 8GB**: runs ROS2 nodes, web server, connects to MCU via USB-UART, reads Realsense D435 camera, reads LD06 Lidar via USB-UART. USB-UART is via CP2104 adapters. Comm between MCU and Jetson uses mavlink protocol.
**MCU = Raspberry Pi Pico W or ESP32S3-Pico**: Controls motors, reads cliff sensors, front and rear sonar, IMU 6050 and wheel encoders.It can override motor commands to prevent collision and it will stop motors in case Jetson commands are timed out.
**Sensors**: The sensors it has are : front - realsense D435 depth camera via USB, LD2450 human tracking lidar. Front and Rear: HC-SR04 sonar. On body:  MPU6050 acceleration sensor and LD06 lidar. 
**Actuators**: 4x 370 type DC brushed motors with encoders. gear ratio 46, pulses per rev 11, hall encoder with forward and reverse sensor pickups.

## Quick Start
0. **Download only the script , edit it and run**
This will clone the repo, install dependencies optionally and create the venv. 
See script ...TODO: Create closing and setup script...temp text below.

1. **Clone the repo**
USERNAME="jeevan" # CHANGE THIS to your Pi's username
EMAIL="jeevanghadge@gmail.com" # CHANGE THIS
GIT_USER="nota2104coda" # CHANGE THIS
REPO_URL="git@github.com:nota2104coda/Jetcar.git"
REPO_DIR="/home/$USERNAME/Jetcar"
git clone --recurse-submodules $REPO_URL $REPO_DIR

2. **create .venv and install local python dependencies**
cd $REPO_DIR
python3 -m venv .venv
source .venv/bin/activate
pip install --upgrade pip
pip install -r jetcar-requirements.txt

3. **Develop ROS2 python nodes for Rpi5/Jetson in** `src/python_pkg/`
   **Develop ROS2 C++ nodes for Jetson in** `src/jetsoncpp_pkg/`
4. **Develop ROS nodes for PC in** `src/pc_pkg`
5. **Develop Pi Pico W code in** `src/mcu/pico-PlatformIO/`
**Develop ESP32S3-Pico code in** `src/mcu/esp32-PlatformIO/`
**Common microcontroller include files** `src/mcu/include/`
6. **Define custom ROS messages in** `src/robot_msgs/msg/` and build with `colcon build`
7. **Access foxglove visualisation** `ws://192.168.50.177:8765` or your chosen IP address. configure this in the foxglove node
8. **Define pins for Pi Pico W** in `libs/arduino/RobotCarPinDefinitionsAndMore.h`
9. **Define robot urdf** in `src/jetsoncpp_pkg/urdf`

## Diagrams
See `docs/designs/wiring/wiring-withANano.fzz` for wiring layouts. TODO: Update circuit considering latest situation.
See `docs/designs/architecture.md` for system and software architecture diagrams (Mermaid format). TODO: Needs update considering Isaac.

## Next Steps
**MCU**
- TODO: Interlocks to ensure MCU stops robot at least 100mm from an obstacle to prevent collision. Also avoids falling down cliff

**Jetson**
- TODO: Implement sensor fusion and navigation logic in ROS2 nodes

## Memory budget Notes
Can your robot run all of this at once?
Absolutely yes on Jetson Orin Nano 8GB — if you tune SLAM + Realsense + Nav2 costmaps.
If you leave everything at defaults = Likely OOM at random times.
7. What to tune (practical)

Realsense
depth resolution: 640×480
fps: 15
decimation filter ON
pointcloud OFF (if not needed)
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

Web server
compress image thumbnails
avoid full-resolution MJPEG
Saves 100–200 MB peak.

⭐ Recommended “safe mode” loadout
If you want reliability and smooth operation:

Run these simultaneously:
Realsense depth 640×480 @ 15 fps
RTAB-Map tuned
Nav2 costmap (voxel layer on)
RPLidar
Arduino sensors
Web UI
Color blob detector for red ball

Avoid:
Running YOLO inference constantly on CPU
Storing large pointcloud histories
Uncompressed rear camera streams
This mode runs in 3.8–4.5 GB comfortably.

---
### Docker Build/Run Instructions

For building docker for Pi5:
```
docker build -f docker/Dockerfile -t picowcar-pi5 --build-arg ENV=pi5 .
```

To build docker for dev env:
```
docker build -f docker/Dockerfile -t picowcar-dev --build-arg ENV=dev .
```

After building your Docker container for development, you can use it as follows:

#### 1. Run the Container Interactively
```
docker run -it --rm \
  --name picowcar-dev \
  -v $(pwd)/pc-src:/app/pc-src \
  -v $(pwd)/config:/app/config \
  -v $(pwd)/logs:/app/logs \
  picowcar-dev
```

#### 2. Run Your Application
If your Dockerfile’s `CMD` is set to run your app, the container will start it automatically. Otherwise, you can run commands inside the container:
```
docker exec -it picowcar-dev /bin/bash
# Then run your Python scripts or tests
```

#### 3. Use Docker Compose (Recommended for Dev)
If you have a `docker-compose.dev.yml`, start your dev environment with:
```
docker-compose -f docker/docker-compose.dev.yml up
```
This will handle volumes, ports, and environment variables for you.

---

**Summary:**  
- Use `docker run` or `docker-compose` to start your dev container.
- Mount your code as volumes for live development.
- Use `docker exec` for interactive work inside the running container.