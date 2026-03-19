# hmi_buttons.msg
#A message to carry motion commands and state from the HMI.

string CMD_FORWARD = "forward"
string CMD_BACKWARD = "backward"
string CMD_CLOCKWISE = "clockwise"
string CMD_COUNTER_CLOCKWISE = "counter_clockwise"
string CMD_STOP = "stop"

#The string command from a joystick or button press.
#It should be set to one of the constants defined above.
string button_motion_cmd

#A boolean to indicate whether to enable or cut power to the motors.
bool motor_power_on

#A boolean to select the control mode:
#'false' for direct button control, 'true' for SmolVLA command.
bool is_smolvla_mode

#A boolean to request the camera feed to be displayed on the webpage.
bool request_camera_feed

#A boolean to request the overlay of detected people on the camera feed.
bool request_camera_overlay