#!/usr/bin/env python3
import serial
import time
import sys

# LD06 UART Settings
PORT = '/dev/ttyTHS1'
BAUD = 230400  # LD06 default

print("=== LD06 LiDAR Diagnostic Tool ===\n")
print("Prerequisites:")
print("1. LD06 5V power connected")
print("2. PWM pin running at 30kHz, 50-70% duty cycle")
print("3. Level shifters on RX/TX lines (5V <-> 3.3V)")
print("4. GND connected between LD06 and Jetson\n")

try:
    print(f"Opening {PORT} at {BAUD} baud...")
    ser = serial.Serial(PORT, BAUD, timeout=1)
    ser.reset_input_buffer()
    
    print("Listening for 10 seconds...\n")
    start = time.time()
    total_bytes = 0
    header_count = 0
    
    while time.time() - start < 10:
        if ser.in_waiting > 0:
            data = ser.read(ser.in_waiting)
            total_bytes += len(data)
            
            # LD06 packet: 0x54 0x2C [46 bytes total]
            for i in range(len(data) - 1):
                if data[i] == 0x54 and data[i+1] == 0x2C:
                    header_count += 1
                    if i + 5 < len(data):
                        speed = (data[i+3] << 8) | data[i+2]
                        start_angle = (data[i+5] << 8) | data[i+4]
                        print(f"✓ Header found! Speed: {speed} deg/s, Start angle: {start_angle/100:.2f}°")
            
            if total_bytes % 100 == 0:
                print(f"  Received {total_bytes} bytes, {header_count} headers")
        
        time.sleep(0.01)
    
    print(f"\n=== Results ===")
    print(f"Total bytes: {total_bytes}")
    print(f"Headers detected: {header_count}")
    print(f"Expected rate: ~4560 bytes/sec (10Hz * 456 bytes)")
    
    if total_bytes == 0:
        print("\n❌ NO DATA - Check:")
        print("  - Is PWM running? (use oscilloscope or LED to verify)")
        print("  - Level shifters connected correctly?")
        print("  - UART TX/RX swapped? (LD06 TX -> Jetson RX)")
        print("  - LD06 getting 5V power?")
    elif header_count == 0:
        print("\n⚠ Data received but no valid headers - Check:")
        print("  - Correct baud rate (should be 230400)")
        print("  - Level shifter quality (signal integrity)")
        print("  First 100 bytes (hex):")
        ser.reset_input_buffer()
        time.sleep(0.1)
        sample = ser.read(100)
        print(f"  {sample.hex(' ')}")
    else:
        print("\n✓ SUCCESS - LD06 is working!")
    
    ser.close()

except serial.SerialException as e:
    print(f"❌ Serial Error: {e}")
    print("Check: sudo usermod -a -G dialout $USER (then logout/login)")
except KeyboardInterrupt:
    print("\n\nInterrupted by user")
except Exception as e:
    print(f"❌ Error: {e}")
