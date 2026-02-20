#!/usr/bin/env python3
import RPi.GPIO as GPIO
import time

# IMPORTANT: Verify your pin number!
# Physical pin 15 = BCM GPIO 22
# Physical pin 32 = BCM GPIO 12
PWM_PIN = 22  # Change to 12 if using physical pin 32

GPIO.setmode(GPIO.BCM)
GPIO.setup(PWM_PIN, GPIO.OUT)

print(f"=== LD06 PWM Test ===")
print(f"Using BCM GPIO {PWM_PIN}")
print("Connect oscilloscope or LED to verify signal\n")

# LD06 specs: 30kHz PWM, 50-70% duty cycle for proper speed
pwm = GPIO.PWM(PWM_PIN, 30000)  # 30 kHz

try:
    for duty in [50, 60, 70]:
        print(f"Testing {duty}% duty cycle (3 seconds)...")
        pwm.start(duty)
        time.sleep(3)
        pwm.stop()
        time.sleep(1)
    
    print("\nSetting optimal 60% duty cycle...")
    pwm.start(60)
    print("PWM running. Press Ctrl+C to stop.")
    
    while True:
        time.sleep(1)

except KeyboardInterrupt:
    print("\nStopping PWM...")
finally:
    pwm.stop()
    GPIO.cleanup()
    print("Cleaned up GPIO")
