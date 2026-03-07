import serial
import time
import sys
import RPi.GPIO as GPIO

# --- GPIO PWM for Motor Control ---
PWM_PIN = 12
GPIO.setmode(GPIO.BCM)
GPIO.setup(PWM_PIN, GPIO.OUT)
pwm = GPIO.PWM(PWM_PIN, 30000)  # 30 kHz
# LiDAR usually spins at 10Hz with ~65% duty cycle.
pwm.start(35.0)  
print(f"Started PWM on GPIO{PWM_PIN} with 65% duty cycle (approx 10Hz)")

port = '/dev/ttyTHS1'
baud_rates = [230400, 115200, 921600]

print(f"Scanning {port} for data at multiple baud rates...")

try:
    for baud in baud_rates:
        print(f"Testing {baud} baud...", end='', flush=True)
        try:
            with serial.Serial(port, baud, timeout=0.1) as ser:
                ser.reset_input_buffer()
                start_time = time.time()
                data_found = False
                
                # Listen for up to 3 seconds per baud rate
                while time.time() - start_time < 3.0:
                    if ser.in_waiting > 0:
                        data = ser.read(ser.in_waiting)
                        if data:
                            print(f" SUCCESS! Read {len(data)} bytes.")
                            print(f"Sample (hex): {data.hex(' ')}")
                            
                            # Detailed check for LD06 protocol
                            # Header: 0x54 0x2C
                            # Structure: 54 2C SpeedL SpeedH ...
                            # We search for the header in the received chunk
                            idx = data.find(b'\x54\x2C')
                            if idx != -1 and idx + 3 < len(data):
                                speed_l = data[idx + 2]
                                speed_h = data[idx + 3]
                                speed_deg_s = (speed_h << 8) | speed_l
                                freq = speed_deg_s / 360.0
                                print(f"Detected LD06 Header! Speed: {speed_deg_s} deg/s ({freq:.2f} Hz)")
                            else:
                                print("Could not find complete LD06 header in this chunk.")
                            
                            # Break inner loop to try next baud rate? 
                            # Or just confirm success?
                            # Let's say we found data and continue monitoring for a bit?
                            # No, let's just confirm success and move on or exit?
                            # The original script continued to next baud rate.
                            data_found = True
                            break
                    time.sleep(0.01)
                
                if not data_found:
                    print(" No data.")
                    
        except serial.SerialException as e:
            print(f" Error: {e}")

except KeyboardInterrupt:
    print("\nAborted by user.")

finally:
    pwm.stop()
    GPIO.cleanup()
    print("Stopped PWM and cleaned up GPIO.")
