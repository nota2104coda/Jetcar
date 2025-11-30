# PicoWCar Project

## Overview
PicoWCar is a 4-wheel robot system using Jetson Orin Nano, Pi Pico W, and Arduino Nano. It fuses camera, lidar, radar, and other sensor data for autonomous navigation and object search, controlled via a web interface. Development will use ROS2 Humble and docker containers for easy dependency management and deployment.
The sensors it has are : front - realsense D435 depth camera via USB, LD2450 human tracking lidar. Rear - rolling shutter CSI camera , 8x8 lidar VL53L5CX. Also a MPU6050 acceleration sensor. Also it has front and rear sonar SR04 type. The sensors are connected to an arduino nano and a raspberry pi pico W . The arduino/pico report back to a jetson orin nano over USB or another suitable bus. For simplicity, I havent mentioned which devices connect to arduino or pico. That's based on simplifying wiring. The pico drives the 4 wheel motors. The jetson orin nano with jetpack 6.2 runs ROS2 nodes to read the data from the arduinos and cameras and pipe back the commands for motion. It also runs a local web server to collect commands from the user. It will also have a rplidar 2D lidar at top.
The purpose of the robot is: when the user asks to navigate to the red football, it scans the room, does SLAM and navigates to the ball and stops at a safe distance. If the path is below a chair, it should intelligently see the height and go below or around. 


## Hardware Architecture
- **Jetson Orin Nano 8GB**: runs ROS2 nodes, web server, connects to Pico W via USB serial or UART or similar bus.
- **Pi Pico W**: Controls motors, reads sensors (radar, lidar, IMU, encoders, SPI display), communicates with Arduino Nano and Jetson Orin Nano
- **Arduino Nano**: Reads ultrasonic and IR cliff sensors, communicates with Pico W


## Quick Start
1. **Clone the repo**
2. **Build Docker container**
   - For PC: `docker-compose -f docker/docker-compose.dev.yml up --build`
   - For Pi5: `docker-compose -f docker/docker-compose.pi5.yml up --build`
3. **Develop ROS2 nodes in `src/pi5_nodes/`**
4. **Define custom messages in `robot_msgs/msg/` and build with `colcon`
5. **Access web interface at `http://localhost:8080`**

## Diagrams
See `docs/diagrams/architecture.md` for system and software architecture diagrams (Mermaid format).

## Next Steps
- Implement sensor fusion and navigation logic in ROS2 nodes
- Integrate serial communication with Pico W
- Expand web interface for visualization
- Flash Pico W and Arduino Mega with respective firmware

## Memory budget
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