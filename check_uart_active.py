import serial
import time
import sys
from pymavlink import mavutil

port = '/dev/ttyUSB0'
# baud_rates = [921600, 115200, 57600]
baud_rates = [921600, 115200] # Try likely ones first to save time

print(f"Active Scanning {port}...")

for baud in baud_rates:
    print(f"Testing {baud} baud (Sending Heartbeat)...", end='', flush=True)
    try:
        # Create a mavlink serial instance
        # we use mavutil.mavlink_connection which handles the serial opening usually, 
        # but let's do it manually to ensure we control the port reset
        conn = mavutil.mavlink_connection(port, baud=baud, source_system=200, source_component=191)
        
        start = time.time()
        data_found = False
        
        # Send a few heartbeats
        for _ in range(3):
            conn.mav.heartbeat_send(
                mavutil.mavlink.MAV_TYPE_GCS,
                mavutil.mavlink.MAV_AUTOPILOT_INVALID,
                0, 0, 0
            )
            
            # Listen for a bit
            sub_start = time.time()
            while time.time() - sub_start < 1.0:
                msg = conn.recv_match(blocking=False)
                if msg:
                    print(f" SUCCESS! Received {msg.get_type()} (ID {msg.get_msgId()})")
                    print(f"  {msg}")
                    data_found = True
                    break
            if data_found:
                break
        
        if not data_found:
            print(" No response.")
            
        conn.close()
                
    except Exception as e:
        print(f" Error: {e}")
