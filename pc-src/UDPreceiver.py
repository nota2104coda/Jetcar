# pico_udp_receiver_terminal.py
# This Python script acts as a UDP server to receive data from the Raspberry Pi Pico W,
# calculates latency, and prints received values to the console.
# Press Ctrl+C in the terminal to stop the script gracefully.

import socket
import struct
import time
import sys # For sys.exit()
#import ntplib
import re

# --- Configuration ---
# !!! IMPORTANT: Must match the PC_PORT defined in the Pico W C++ code !!!
UDP_PORT = 12345
# Listen on all available interfaces. If you have issues, replace '0.0.0.0' with
# your PC's actual IP address (e.g., '192.168.1.100').
UDP_IP = '0.0.0.0'

# The size of the expected data packet from Pico
# Pico sends: uint64_t (8 bytes) + float (4 bytes) = 12 bytes
# Format string: "<Qf" means Little-endian (<), Unsigned long long (Q), Float (f)
PACKET_FORMAT = "<Qf"
PACKET_SIZE = struct.calcsize(PACKET_FORMAT)
NTP_TO_UNIX_EPOCH_OFFSET_SECONDS = 2208988800
# Setup UDP socket
sock = None # Initialize sock to None
try:
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM) # Create a UDP socket
    sock.bind((UDP_IP, UDP_PORT)) # Bind the socket to the specified IP and port
    sock.settimeout(1.0) # Set a timeout for recvfrom (e.g., 1 second)
    print(f"Listening for UDP packets on {UDP_IP}:{UDP_PORT}...")
except socket.error as e:
    print(f"Error setting up UDP socket: {e}")
    print("Ensure the port is not already in use and your firewall allows UDP traffic.")
    if sock:
        sock.close() # Close socket if it was partially created
    sys.exit(1) # Exit the script if socket setup fails

# Main loop to continuously receive and print data
try:
    while True:
        try:
            # Attempt to receive data with a timeout
            data, addr = sock.recvfrom(1024) # Buffer size (can be larger than PACKET_SIZE)
            # Get PC's current timestamp immediately after receiving data (in microseconds)
            receive_time_us = int(time.time() * 1_000_000)

            # Unpack the received binary data
            if len(data) == PACKET_SIZE:
                pico_timestamp_us, vx_value = struct.unpack(PACKET_FORMAT, data)
                latency_us = receive_time_us - (pico_timestamp_us - NTP_TO_UNIX_EPOCH_OFFSET_SECONDS * 1000000)
                latency_ms = latency_us / 1000.0

                # Print received values to the terminal
                print(f"Received from {addr}: Pico_TS={pico_timestamp_us} us, Vx={vx_value}, Latency={latency_ms:.2f} ms")

            else:
                print(f"Received malformed packet of size {len(data)} from {addr}. Expected {PACKET_SIZE} bytes.")

        except socket.timeout:
            # This is expected if no data is received within the timeout period
            # You can add a message here if you want to indicate inactivity
            # print("Waiting for data...")
            pass
        except Exception as e:
            print(f"An error occurred during data reception or processing: {e}")

except KeyboardInterrupt:
    print("\nScript terminated by user (Ctrl+C).")
finally:
    # Ensure the UDP socket is closed when the script exits
    if sock:
        sock.close()
        print("UDP socket closed.")
    sys.exit(0) # Exit cleanly
