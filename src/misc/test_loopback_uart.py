#!/usr/bin/env python3
"""
UART Loopback test - short pin 8 (TX) to pin 10 (RX) on J12 header
"""
import serial
import time

PORT = '/dev/ttyTHS1'
BAUD = 230400

print("=== UART Loopback Test ===")
print("INSTRUCTIONS:")
print("1. Disconnect LD06 from UART pins")
print("2. Short UART_A TX (Pin 8) to RX (Pin 10) with a jumper wire")
print("3. Press Enter when ready...")
input()

try:
    ser = serial.Serial(PORT, BAUD, timeout=1)
    print("✓ Port opened\n")
    
    # Send test pattern
    test_data = bytes([0x54, 0x2C, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08])
    
    print("Sending test pattern:", test_data.hex(' '))
    ser.reset_input_buffer()
    ser.write(test_data)
    ser.flush()
    time.sleep(0.1)
    
    # Read back
    received = ser.read(len(test_data))
    print("Received back:     ", received.hex(' '))
    
    if received == test_data:
        print("\n✓ LOOPBACK SUCCESS - UART hardware is working!")
        print("\nThis means the problem is:")
        print("  - LD06 TX not connected to Jetson RX")
        print("  - LD06 TX pin damaged/not working")
        print("  - LD06 not powered correctly")
    elif len(received) == 0:
        print("\n❌ NO DATA - TX/RX not looped back correctly")
        print("Check your jumper wire connection")
    else:
        print("\n⚠ DATA MISMATCH - possible electrical issue")
    
    ser.close()

except Exception as e:
    print(f"❌ Error: {e}")
    import traceback
    traceback.print_exc()
