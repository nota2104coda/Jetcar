import serial
import time

PORT = '/dev/ttyUSB0'
BAUD = 921600

def test_loopback():
    print(f"Opening {PORT} at {BAUD} for loopback test...")
    try:
        ser = serial.Serial(PORT, BAUD, timeout=1.0)
    except serial.SerialException as e:
        print(f"Error opening port: {e}")
        return

    message = b'Hello Loopback!'
    print(f"Sending: {message}")
    ser.write(message)
    ser.flush()
    
    time.sleep(0.1)
    
    if ser.in_waiting > 0:
        received = ser.read(ser.in_waiting)
        print(f"Received: {received}")
        if message in received:
            print("Loopback TEST PASSED!")
        else:
            print("Loopback TEST FAILED: Data mismatch or partial data.")
    else:
        print("Loopback TEST FAILED: No data received.")
        print("Ensure TX is connected to RX on the adapter.")

    ser.close()

if __name__ == "__main__":
    test_loopback()
