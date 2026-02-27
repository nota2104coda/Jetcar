#!/usr/bin/env python3
import RPi.GPIO as GPIO
import time
import signal
import sys

PWM_PIN = 12
FREQUENCY = 30000 
DUTY_CYCLE = 65.0 # 65% for approx 10Hz

def cleanup(sig, frame):
    print("Stopping LIDAR PWM")
    if 'pwm' in globals():
        pwm.stop()
    GPIO.cleanup()
    sys.exit(0)

def main():
    global pwm
    
    # Handle termination signals
    signal.signal(signal.SIGINT, cleanup)
    signal.signal(signal.SIGTERM, cleanup)
    
    print(f"Starting LIDAR PWM on GPIO{PWM_PIN}")
    try:
        GPIO.setmode(GPIO.BCM)
        GPIO.setup(PWM_PIN, GPIO.OUT)
        pwm = GPIO.PWM(PWM_PIN, FREQUENCY)
        pwm.start(DUTY_CYCLE)
        
        # Keep running to maintain software PWM
        while True:
            time.sleep(1)
            
    except Exception as e:
        print(f"Error initializing PWM: {e}")
        cleanup(None, None)

if __name__ == '__main__':
    main()
