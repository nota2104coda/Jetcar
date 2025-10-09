# PicoWCar Project

## Overview
PicoWCar is a multi-platform robot system using Raspberry Pi 5, Pi Pico W, and Arduino Mega. It fuses camera, lidar, radar, and other sensor data for autonomous navigation and object search, controlled via a web interface. Development is done inside Docker containers for easy dependency management and deployment.

## Hardware Architecture
- **Raspberry Pi 5**: CSI camera, runs ROS2 nodes, web server, connects to Pico W via USB serial
- **Pi Pico W**: Controls motors, reads sensors (radar, lidar, IMU, encoders, SPI display), communicates with Arduino Mega and Pi5
- **Arduino Mega**: Reads ultrasonic and IR cliff sensors, communicates with Pico W

## Software Architecture
- **ROS2 Nodes (on Pi5/PC)**:
  - `vision_node`: Sensor fusion, VLA model (e.g., SmolVLA)
  - `navigation_node`: Path planning
  - `control_node`: Serial comms to Pico W
  - `web_server_node`: Web interface for commands and visualization
- **Custom ROS2 Messages**: See `robot_msgs/msg/`

## Directory Structure
```
PicoWCar/
├── docker/
├── src/
│   └── pi5_nodes/
├── robot_msgs/
│   └── msg/
├── docs/
│   └── diagrams/
├── README.md
```

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