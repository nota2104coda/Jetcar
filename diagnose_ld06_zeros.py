#!/usr/bin/env python3
"""
Comprehensive LD06 diagnostic - all zeros problem solver
"""
import serial
import time

PORT = '/dev/ttyTHS1'
BAUD = 230400

print("=== LD06 All-Zeros Diagnostic ===\n")
print("Current problem: Receiving data but all 0x00 bytes")
print("This means: UART RX pin is stuck LOW (0V)\n")

print("JETSON ORIN NANO 40-PIN HEADER - UART_A (ttyTHS1):")
print("  Pin 8  (GPIO14): UART_A TX (Jetson sends)")
print("  Pin 10 (GPIO15): UART_A RX (Jetson receives) ← LD06 TX goes here")
print()
print("LD06 CONNECTOR:")
print("  VCC: 5V power")
print("  TX:  3.3V output (connect to Jetson Pin 10)")
print("  RX:  3.3V input (connect to Jetson Pin 8)")  
print("  GND: Ground")
print("  PWM: 3.3V input (optional, motor runs without it)")
print()

# Test 1: Read current state
try:
    print("TEST 1: Reading current UART state...")
    ser = serial.Serial(PORT, BAUD, timeout=1)
    ser.reset_input_buffer()
    time.sleep(0.2)
    
    data = ser.read(200)
    print(f"  Bytes: {len(data)}")
    print(f"  Sample: {data[:50].hex(' ')}")
    
    # Analyze
    if len(data) == 0:
        print("  ❌ No data - UART not receiving anything")
        print("     Check: Is LD06 powered? Is motor spinning?")
    elif all(b == 0x00 for b in data):
        print("  ❌ All zeros - RX line stuck LOW")
        print("     LIKELY CAUSES:")
        print("     1. LD06 TX not connected to Jetson Pin 10 (RX)")
        print("     2. Wire broken/loose on LD06 TX line")
        print("     3. LD06 TX pin damaged")
        print("     4. Connected to wrong pin on Jetson")
    elif all(b == 0xFF for b in data):
        print("  ❌ All 0xFF - RX line stuck HIGH (idle)")
        print("     LIKELY CAUSE: LD06 not transmitting (not powered or broken)")
    else:
        print("  ✓ Varied data - signal present!")
        
    ser.close()
    
    # Test 2: Check wiring instructions
    print("\nTEST 2: Wiring verification checklist")
    print("  [ ] LD06 VCC → 5V power source")
    print("  [ ] LD06 GND → Jetson GND (Pin 6, 9, 14, 20, 25, 30, 34, or 39)")
    print("  [ ] LD06 TX  → Jetson Pin 10 (UART_A RX)")
    print("  [ ] LD06 RX  → Jetson Pin 8 (UART_A TX) [if you want 2-way comm]")
    print()
    
    # Test 3: Suggest loopback
    print("TEST 3: Hardware verification")
    print("  To verify Jetson UART is working:")
    print("  1. Disconnect LD06")
    print("  2. Use jumper wire: connect Pin 8 to Pin 10")
    print("  3. Run: python3 test_loopback_uart.py")
    print()
    
    # Test 4: Multimeter check
    print("TEST 4: Voltage check (use multimeter)")
    print("  With LD06 powered and spinning:")
    print("  1. Measure LD06 TX to GND: should show ~3.3V or fluctuating")
    print("  2. If stuck at 0V: LD06 TX output is dead/damaged")
    print("  3. If ~3.3V: check if wire reaches Jetson Pin 10")
    print()
    
    # Test 5: Alternative - try UART E
    print("TEST 5: Try alternative UART (UART_E / ttyTHS2)")
    print("  If UART_A doesn't work, try UART_E:")
    print("  Pin 11 (GPIO17): UART_E TX")
    print("  Pin 36 (GPIO16): UART_E RX ← connect LD06 TX here instead")
    print("  Then use: PORT = '/dev/ttyTHS2'")
    
except Exception as e:
    print(f"❌ Error: {e}")
    import traceback
    traceback.print_exc()

print("\n" + "="*60)
print("MOST LIKELY FIX:")
print("  Your LD06 TX wire is NOT connected to Jetson Pin 10")
print("  Double-check the physical connection with a multimeter")
print("="*60)
