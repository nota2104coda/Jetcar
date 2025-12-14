 I want to build a robot with vSLAM and navigation. It has 4 wheel drive and no steering. As such, it will use the 4 independent motors to do tank turns. It will be able to use its realsense 435 camera to see and YOLO to identify objects. 

The purpose of the robot is: 
-when the user asks to navigate to the red football, it scans the room, identifies where the red football is. It does SLAM and navigates to the ball and stops at a safe distance. 
-If the path is below a chair, it should intelligently see the height and go below or around. If the object is moving, still the robot should keep tracking and following it.
-During any movements, it should use the camera and YOLO to identify any objects in or entering its path. For instance, if I roll another blue ball down its path, it should spot and mark its cost function in the SLAM maps and ensure to avoid collision with said blue ball. 
-It should also use the LD2450 human detecting radar to avoid colliding with humans.
User interface:
-The user input should be via a web page or a foxglove interface that is accessible to other devices on the home network. i.e. the service will run on the jetson orin nano computer. The UI will run on my laptop or mobile phone in the same Wifi network. 
-The user should be able to select manual/auto controls. 
-In manual control, there should be buttons for forward, back, turn left, turn right. In auto control, the site should have a text box to enter the name of the object being targeted. A red STOP button should stop the motors immediately.

The detail:
-the raspberry pi pico works in realtime and ensures basic collision avoidance and safety checks. 
-The pico will monitor the inputs from non-realtime ROS2 computer, which is the jetson orin nano.
-the Jetson will process the sensor inputs and do SLAM and NAV. It will send commands to the Pico. The Pico will arbitrate these against basic collision logic from its sonar and cliff sensor and the human radar.
- Sensors read by both Jetson and Pico: the I2C sensors such as MPU6050 IMU, 8x8 lidar VL53L5X. 
- Sensor read by Jetson only: the LD06 Lidar, the Realsense camera
- Sensors read by Pico and shared with Jetson: Cliff sensors front and back, sonar HCSR04 at back, human sensing radar LD2450.