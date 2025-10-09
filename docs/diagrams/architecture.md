# System Architecture Diagram

```mermaid
graph TD
    PC[PC/WSL2] -- Ethernet/WiFi --> Pi5[Raspberry Pi 5]
    Pi5 -- USB Serial --> PicoW[Pico W]
    PicoW -- UART --> Mega[Arduino Mega]
    Pi5 -- CSI Camera --> Camera[RPi Camera]
    Pi5 -- Web Server --> Web[Web Browser]
    PicoW -- UART --> Radar[LD2450 Radar]
    PicoW -- UART --> Lidar[VL53L5X 8x8 Lidar]
    PicoW -- UART --> IMU[MPU6050]
    PicoW -- UART --> Servo[Servo Controller]
    PicoW -- Motor Drivers --> Motors[4 Wheel Motors]
    PicoW -- Encoder Pins --> Encoders[Wheel Encoders]
    PicoW -- SPI --> Display[SPI Display]
    Mega -- Ultrasonic --> USensors[Ultrasonic Sensors]
    Mega -- IR --> IRSensors[IR Cliff Sensors]
```

# Software Architecture Diagram

```mermaid
flowchart TD
    subgraph ROS2_Nodes[ROS2 Nodes (Pi5/PC)]
        Vision[vision_node: Camera, Lidar, Radar Fusion, VLA]
        Navigation[navigation_node: Path Planning]
        Control[control_node: Serial Comms to Pico W]
        WebServer[web_server_node: Web Interface]
    end
    subgraph PicoW_Arduino[Pico W (Arduino IDE)]
        PicoWSensors[Sensor Reading]
        PicoWMotors[Motor Control]
        PicoWComms[UART/SPI Comms]
        PicoWSerial[Serial Bridge to Pi5]
    end
    subgraph Mega_Arduino[Arduino Mega]
        MegaSensors[Sensor Reading]
        MegaComms[UART to Pico W]
    end
    Vision -- Sensor Data --> Navigation
    Navigation -- Motor Commands --> Control
    Control -- Serial Data --> PicoWSerial
    PicoWSerial -- Sensor Data --> Control
    WebServer -- Commands --> Navigation
    WebServer -- Visualization --> Vision
```
