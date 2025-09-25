#Standard ROS2 CompressedImage message for the video feed
sensor_msgs/msg/CompressedImage image

#Additional metadata specific to the Raspberry Pi Camera Module 3
float32 digital_gain
float32 analog_gain
float32 exposure_time_us
uint32 awb_mode
uint32 flicker_mode