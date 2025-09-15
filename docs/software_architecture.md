# Software Architecture for Robot Rover

## Overview
This document details the software architecture for the robot rover project. It includes ROS2 nodes, topics, services, and Docker containers.

---

## Software Architecture Diagram

```plaintext
+-------------------------------------------------------------+
|                         Web Interface                       |
|  - Hosted on Pi5                                            |
|  - Communicates with ROS2 Web Server Node                  |
+-------------------------------------------------------------+
                             |
                             v
+-------------------------------------------------------------+
|                     ROS2 Web Server Node                   |
|  - Hosted on Pi5                                            |
|  - Handles HTTP requests and publishes commands to ROS2     |
|  - Topic: /user_commands                                    |
+-------------------------------------------------------------+
                             |
                             v
+-------------------------------------------------------------+
|                     ROS2 Control Node                      |
|  - Hosted on Pi5                                            |
|  - Subscribes to /user_commands                             |
|  - Publishes motor and servo commands to /actuator_cmds     |
|  - Topic: /actuator_cmds                                    |
+-------------------------------------------------------------+
                             |
                             v
+-------------------------------------------------------------+
|                     ROS2 Navigation Node                   |
|  - Hosted on Pi5                                            |
|  - Subscribes to /sensor_data                               |
|  - Publishes navigation commands to /actuator_cmds          |
|  - Topic: /sensor_data                                      |
|  - Topic: /actuator_cmds                                    |
+-------------------------------------------------------------+
                             |
                             v
+-------------------------------------------------------------+
|                     ROS2 Vision Node                       |
|  - Hosted on Pi5                                            |
|  - Processes CSI camera data                                |
|  - Publishes vision data to /sensor_data                   |
|  - Topic: /sensor_data                                      |
+-------------------------------------------------------------+
                             |
                             v
+-------------------------------------------------------------+
|                     ROS2 Sensor Node                       |
|  - Hosted on Pi Pico W                                      |
|  - Publishes sensor data to /sensor_data                   |
|  - Topic: /sensor_data                                      |
+-------------------------------------------------------------+
                             |
                             v
+-------------------------------------------------------------+
|                     ROS2 Actuator Node                     |
|  - Hosted on Pi Pico W                                      |
|  - Subscribes to /actuator_cmds                             |
|  - Controls motors and servos                               |
|  - Topic: /actuator_cmds                                    |
+-------------------------------------------------------------+
```

---

## ROS2 Nodes

### 1. **Web Server Node**
- **Role**: Handles HTTP requests from the web interface and publishes commands to ROS2.
- **Topic**: `/user_commands`

### 2. **Control Node**
- **Role**: Subscribes to user commands and publishes actuator commands.
- **Topic**: `/user_commands`, `/actuator_cmds`

### 3. **Navigation Node**
- **Role**: Subscribes to sensor data and publishes navigation commands.
- **Topic**: `/sensor_data`, `/actuator_cmds`

### 4. **Vision Node**
- **Role**: Processes CSI camera data and publishes vision data.
- **Topic**: `/sensor_data`

### 5. **Sensor Node**
- **Role**: Publishes sensor data from Pi Pico W.
- **Topic**: `/sensor_data`

### 6. **Actuator Node**
- **Role**: Subscribes to actuator commands and controls motors and servos.
- **Topic**: `/actuator_cmds`

---

## Docker Containers

### 1. **ROS2 Core**
- **Description**: Base ROS2 environment.
- **Hosted On**: Pi5

### 2. **Web Server**
- **Description**: Hosts the web interface and ROS2 Web Server Node.
- **Hosted On**: Pi5

### 3. **Sensor and Actuator Nodes**
- **Description**: Runs ROS2 Sensor and Actuator Nodes.
- **Hosted On**: Pi Pico W

---

## Next Steps
1. Set up the ROS2 minimal environment on WSL2 and Pi5.
2. Create Docker containers for ROS2 development and deployment.
3. Define the directory structure for the project.