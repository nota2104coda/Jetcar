# This message is used to send PWM control signals from the pico_control_node
# to the Raspberry Pi Pico.

# It controls four wheel motors and two camera mount servos.

# PWM values for the four wheel motors.
# The order can be conventional, e.g., [front_left, front_right, rear_left, rear_right]. the unit is percent duty cycle
int32[4] motor_pwms

# PWM values for the two camera mount servos.
# The order can be, e.g., [pan_servo, tilt_servo] the unit is percent duty cycle
int8 pan_servo_pwm
int8 tilt_servo_pwm
