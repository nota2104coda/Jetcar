# System Architecture for Robot Rover

## Overview
This document outlines the high-level system architecture for the robot rover project. The architecture includes hardware components, communication protocols, and software layers.

---

## System Architecture Diagram

```plaintext
+-------------------------------------------------------------+
|                         Web Interface                       |
|  (User commands, visualization, and monitoring)             |
+-------------------------------------------------------------+
                             |
                             v
+-------------------------------------------------------------+
|                     Jetson Orin Nano                       |
|  - ROS2 Nodes: Web Server, Navigation, Vision, Control     |
|  - I2C Communication with Pi Pico W                        |
|  - Dockerized ROS2 Environment                             |
+-------------------------------------------------------------+
                             |
                             v
+-------------------------------------------------------------+
|                        Pi Pico W                            |
|  - Sensor Integration:                                      |
|    - LD2450 Radar (UART)                                    |
|    - VL53L5X Lidar (UART)                                   |
|    - MPU6050 (UART)                                         |
|  - Actuator Control:                                        |
|    - Motor Drivers (PWM)                                    |
|    - Servo for Camera Mount (PWM)                           |
|  - SPI Display for Debugging                                |
|  - Communication with Arduino Mega (UART)                   |
+-------------------------------------------------------------+
                             |
                             v
+-------------------------------------------------------------+
|                     Arduino Mega                           |
|  - Sensor Integration:                                     |
|    - Ultrasonic Sensors (Front/Rear)                       |
|    - Infrared Cliff Sensors (Front/Rear)                   |
|  - Communication with Pi Pico W (UART)                     |
+-------------------------------------------------------------+
```

---

## Key Interactions

### 1. **Web Interface**
- Hosted on the Raspberry Pi 5.
- Allows users to send commands (e.g., "Find the blue ball") and view real-time data.

### 2. **Raspberry Pi 5**
- Acts as the central processing unit.
- Runs ROS2 nodes for navigation, vision, and control.
- Processes camera data from the CSI camera.
- Communicates with Pi Pico W via USB serial.

### 3. **Pi Pico W**
- Handles low-level sensor data acquisition and actuator control.
- Integrates radar, lidar, and IMU sensors.
- Controls motor drivers and servo for the camera mount.
- Displays debugging information on the SPI screen.
- Communicates with Arduino Mega for additional sensor data.

### 4. **Arduino Mega**
- Manages ultrasonic and infrared sensors.
- Sends sensor data to Pi Pico W via UART.

---

## Communication Protocols

- **USB Serial**: Between Raspberry Pi 5 and Pi Pico W.
- **UART**: For communication between Pi Pico W and Arduino Mega, and for connecting sensors to Pi Pico W.
- **SPI**: For the debugging display on Pi Pico W.
- **ROS2 Topics**: For inter-node communication on Raspberry Pi 5.

---

## Next Steps
1. Finalize the software architecture, including ROS2 nodes, topics, and services.
2. Define the directory structure for the project.
3. Begin setting up the ROS2 environment on WSL2 and Raspberry Pi 5.