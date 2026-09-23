#!/usr/bin/env python3
import time
import subprocess
import Jetson.GPIO as GPIO

# Pin Definition (Using Board numbering scheme)
# Choose physical pins that are easy to access and have internal pull-ups or use external pull-ups
START_PIN = 11  # Physical Pin 11 (GPIO 17 / Pin 11 on Orin Nano/NX and Jetson Nano)
STOP_PIN = 13   # Physical Pin 13 (GPIO 27 / Pin 13)
GREEN_LED = 15  # LED indicating ROS2 is running
RED_LED = 16    # LED indicating ROS2 is stopped

SERVICE_NAME = "jetcar.service"

def setup():
    GPIO.setmode(GPIO.BOARD)
    # Configure input buttons with internal pull-up resistors
    GPIO.setup(START_PIN, GPIO.IN, pull_up_down=GPIO.PUD_UP)
    GPIO.setup(STOP_PIN, GPIO.IN, pull_up_down=GPIO.PUD_UP)
    # Configure status LEDs
    GPIO.setup(GREEN_LED, GPIO.OUT, initial=GPIO.LOW)
    GPIO.setup(RED_LED, GPIO.OUT, initial=GPIO.HIGH)

def is_service_running():
    try:
        res = subprocess.run(["systemctl", "is-active", SERVICE_NAME], stdout=subprocess.PIPE, text=True)
        return res.stdout.strip() == "active"
    except Exception:
        return False

def control_service(action):
    try:
        # systemctl start/stop requires root. This script should be run as root
        # or via sudo NOPASSWD configs.
        subprocess.run(["systemctl", action, SERVICE_NAME])
    except Exception as e:
        print(f"Error executing {action}: {e}")

def main():
    setup()
    print("Jetcar Hardware Launch Daemon started. Monitoring buttons...")
    
    last_start_state = GPIO.HIGH
    last_stop_state = GPIO.HIGH
    
    while True:
        # Read button inputs (Active LOW because of pull-up)
        start_state = GPIO.input(START_PIN)
        stop_state = GPIO.input(STOP_PIN)
        
        # Detect press (transition from HIGH to LOW)
        if start_state == GPIO.LOW and last_start_state == GPIO.HIGH:
            print("Start button pressed! Starting ROS2 service...")
            # Blink LEDs to indicate transition
            for _ in range(3):
                GPIO.output(GREEN_LED, GPIO.HIGH)
                GPIO.output(RED_LED, GPIO.HIGH)
                time.sleep(0.1)
                GPIO.output(GREEN_LED, GPIO.LOW)
                GPIO.output(RED_LED, GPIO.LOW)
                time.sleep(0.1)
            
            control_service("start")
            time.sleep(0.5)  # debounce
            
        elif stop_state == GPIO.LOW and last_stop_state == GPIO.HIGH:
            print("Stop button pressed! Stopping ROS2 service...")
            control_service("stop")
            time.sleep(0.5)  # debounce
            
        # Update LED indicator states based on actual service status
        running = is_service_running()
        if running:
            GPIO.output(GREEN_LED, GPIO.HIGH)
            GPIO.output(RED_LED, GPIO.LOW)
        else:
            GPIO.output(GREEN_LED, GPIO.LOW)
            GPIO.output(RED_LED, GPIO.HIGH)
            
        last_start_state = start_state
        last_stop_state = stop_state
        
        time.sleep(0.05)

if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        GPIO.cleanup()
    finally:
        GPIO.cleanup()
