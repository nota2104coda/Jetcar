#!/usr/bin/env python3
"""
Simple LD06 test - no PWM needed since motor spins by itself
"""
import serial
import time

PORT = '/dev/ttyTHS1'
BAUD = 230400

print("=== Simple LD06 UART Test ===")
print(f"Port: {PORT}, Baud: {BAUD}")
print("Motor should be spinning already (no PWM needed)\n")

try:
    ser = serial.Serial(PORT, BAUD, timeout=2)
    print("✓ Port opened successfully\n")
    
    # Clear any stale data
    ser.reset_input_buffer()
    time.sleep(0.2)
    
    print("Reading raw data for 5 seconds...")
    start = time.time()
    total_bytes = 0
    raw_samples = []
    
    while time.time() - start < 5:
        if ser.in_waiting > 0:
            chunk = ser.read(ser.in_waiting)
            total_bytes += len(chunk)
            if len(raw_samples) < 3:
                raw_samples.append(chunk[:50])  # Save first 50 bytes of first 3 chunks
            print(f"  [{time.time()-start:.1f}s] Received {len(chunk)} bytes")
        time.sleep(0.05)
    
    print(f"\n=== Results ===")
    print(f"Total bytes received: {total_bytes}")
    print(f"Expected: ~4560 bytes (230400 baud, 10Hz lidar)")
    
    if total_bytes == 0:
        print("\n❌ NO DATA RECEIVED!")
        print("\nTroubleshooting steps:")
        print("1. Check wiring:")
        print("   - LD06 TX → Jetson UART_A RX (Pin 10)")
        print("   - LD06 GND → Jetson GND")
        print("   - LD06 VCC → 5V power")
        print("2. Verify UART_A is enabled in device tree")
        print("3. Try loopback test: short TX and RX on Jetson")
        print("4. Check if motor is actually spinning (power connected?)")
    else:
        print(f"\n✓ Receiving data! ({total_bytes/5:.0f} bytes/sec)")
        print("\nFirst raw samples (hex):")
        for i, sample in enumerate(raw_samples):
            print(f"  Sample {i+1}: {sample.hex(' ')[:120]}...")
        
        # Check for LD06 header pattern
        print("\nSearching for LD06 headers (0x54 0x2C)...")
        ser.reset_input_buffer()
        time.sleep(0.1)
        
        data = ser.read(1000)
        headers = []
        for i in range(len(data) - 1):
            if data[i] == 0x54 and data[i+1] == 0x2C:
                headers.append(i)
        
        if headers:
            print(f"✓ Found {len(headers)} valid LD06 headers!")
            print("✓ LD06 IS WORKING CORRECTLY!")
            
            # Parse first header
            if len(data) >= headers[0] + 6:
                idx = headers[0]
                speed = (data[idx+3] << 8) | data[idx+2]
                start_angle = (data[idx+5] << 8) | data[idx+4]
                print(f"\nFirst packet:")
                print(f"  Speed: {speed} deg/s ({speed/360:.1f} Hz)")
                print(f"  Start angle: {start_angle/100:.2f}°")
        else:
            print("⚠ No valid headers found - data is corrupted")
            print("Possible issues:")
            print("  - Wrong baud rate")
            print("  - Electrical noise/bad connection")
            print("  - TX/RX pins swapped")
    
    ser.close()

except serial.SerialException as e:
    print(f"❌ Serial Error: {e}")
except KeyboardInterrupt:
    print("\n\nInterrupted")
except Exception as e:
    print(f"❌ Error: {e}")
    import traceback
    traceback.print_exc()
