import serial
import time
import sys

port = '/dev/ttyUSB0'
baud_rates = [921600, 115200, 57600, 460800, 230400, 9600]

print(f"Scanning {port} for data...")

for baud in baud_rates:
    print(f"Testing {baud} baud...", end='', flush=True)
    try:
        with serial.Serial(port, baud, timeout=0.1) as ser:
            ser.reset_input_buffer()
            start = time.time()
            data_found = False
            while time.time() - start < 3.0: # Listen for 3 seconds
                if ser.in_waiting > 0:
                    data = ser.read(ser.in_waiting)
                    if data:
                        print(f" SUCCESS! Read {len(data)} bytes.")
                        print(f"Sample (hex): {data[:20].hex(' ')}")
                        try:
                            # Try to decode as ascii, ignore errors
                            print(f"Sample (text): {data[:20].decode('utf-8', errors='ignore')}")
                        except:
                            pass
                        data_found = True
                        break
                time.sleep(0.01)
            
            if not data_found:
                print(" No data.")
                
    except serial.SerialException as e:
        print(f" Error: {e}")
