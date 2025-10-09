# Processing Python Mode - Sine Wave Plotter
# Receives sine wave values from Raspberry Pi Pico W via UDP
# and plots them.

import socket # Import Python's socket library for UDP
import struct # Import struct for unpacking binary data

# --- UDP Configuration ---
# !!! IMPORTANT: Set this to the port your Pico is sending data to (PC_PORT) !!!
UDP_PORT = 12345
# For UDP_IP, use an empty string "" to listen on all available network interfaces
# on your PC. If you have multiple network adapters and want to bind to a specific one,
# use your PC's actual IP address (e.g., "192.168.1.100").
UDP_IP = "192.168.0.20" # Listen on all interfaces
# The size of the expected data packet from Pico
# Pico sends: uint64_t (8 bytes) + float (4 bytes) = 12 bytes
# Format string: "<Qf" means Little-endian (</), Unsigned long long (Q), Float (f)
PACKET_FORMAT = "<Qf"
PACKET_SIZE = struct.calcsize(PACKET_FORMAT)

# UDP socket object
sock = None

# Plotting variables
history = []          # List to store recent sine wave values
MAX_HISTORY_LENGTH = 500 # Number of points to keep in history
current_value = 0.0
current_timestamp = 0 # To store the timestamp, though not plotted directly

# Define the expected range of your sine wave
# Your Pico code calculates: AMPLITUDE * sinf(2.0f * M_PI * FREQUENCY * time_elapsed)
# With AMPLITUDE = 100.0f, the sine wave will range from -100.0 to +100.0.
MIN_PLOT_VALUE = -100.0
MAX_PLOT_VALUE = 100.0

def setup():
    global sock, history
    size(800, 400) # Window size (width, height)
    background(20) # Dark background
    frameRate(60)  # Try to update at 60 frames per second

    print("Setting up UDP listener...")
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM) # UDP socket
        sock.bind((UDP_IP, UDP_PORT)) # Bind to listen for incoming data
        sock.setblocking(False) # Set to non-blocking mode so draw() doesn't freeze
        print(f'Listening for UDP data on {UDP_IP}:{UDP_PORT}')
    except socket.error as e:
        print(f'Error setting up UDP socket: {e}')
        print("Ensure the port is not already in use and your firewall allows UDP traffic.")
        exit() # Exit if UDP setup fails

    # Initialize history list
    for _ in range(MAX_HISTORY_LENGTH):
        history.append(0.0) # Fill with zeros initially

def draw():
    global history, current_value, current_timestamp

    background(20) # Clear the background each frame

    # --- Receive UDP Data ---
    try:
        # Try to receive a packet. If no data, it will raise a BlockingIOError
        # because the socket is non-blocking.
        data, addr = sock.recvfrom(PACKET_SIZE)

        # Unpack the binary data
        # The result is a tuple (timestamp, vx_value)
        timestamp_us, vx_value = struct.unpack(PACKET_FORMAT, data)

        # Store the current values
        current_timestamp = timestamp_us
        current_value = vx_value

        # Print the received value to the console
        print(r"Received from {addr}: Timestamp={timestamp_us} us, Vx={vx_value:.2f}")

        # Add the new value to the history list
        history.append(vx_value)
        # Keep the history list at a fixed length
        if len(history) > MAX_HISTORY_LENGTH:
            history.pop(0) # Remove the oldest value

    except socket.error as e:
        # If no data is available (EWOULDBLOCK or EAGAIN), this is expected in non-blocking mode.
        # Other socket errors (like connection reset) might also occur.
        if e.errno == 10035: # Windows specific error for EWOULDBLOCK (no data available)
            pass # No data received yet, just continue drawing
        elif e.errno == 11: # Linux/macOS specific error for EAGAIN (no data available)
            pass # No data received yet, just continue drawing
        else:
            print(r"UDP receive error: {e}")
            # You might want to add more robust error handling or simply ignore transient errors

    # --- Drawing Logic (remains similar to serial version) ---

    # Draw grid lines
    stroke(50)
    for i in range(0, width, 50):
        line(i, 0, i, height)
    for i in range(0, height, 50):
        line(0, i, width, i)

    # Draw plot area lines for reference (center, min, max)
    stroke(100)
    # Line for the center (0.0 for sine wave)
    y_center_line = map(0.0, MIN_PLOT_VALUE, MAX_PLOT_VALUE, height, 0)
    line(0, y_center_line, width, y_center_line)
    # Max value line
    y_max_line = map(MAX_PLOT_VALUE, MIN_PLOT_VALUE, MAX_PLOT_VALUE, height, 0)
    line(0, y_max_line, width, y_max_line)
    # Min value line
    y_min_line = map(MIN_PLOT_VALUE, MIN_PLOT_VALUE, MAX_PLOT_VALUE, height, 0)
    line(0, y_min_line, width, y_min_line)

    # Plot the history data
    stroke(0, 200, 0) # Green color for the plot line
    strokeWeight(2)

    for i in range(len(history) - 1):
        # Map the sensor value to a y-coordinate on the screen
        y1 = map(history[i], MIN_PLOT_VALUE, MAX_PLOT_VALUE, height, 0) # Invert y-axis for plotting
        y2 = map(history[i + 1], MIN_PLOT_VALUE, MAX_PLOT_VALUE, height, 0)

        # Draw lines between points
        # Scale x-axis to fit all history points
        x1 = map(i, 0, MAX_HISTORY_LENGTH - 1, 0, width)
        x2 = map(i + 1, 0, MAX_HISTORY_LENGTH - 1, 0, width)
        line(x1, y1, x2, y2)

    # Display current value and timestamp
    fill(255)
    textSize(20)
    text(f"Current Value: {current_value:.2f}", 10, 30)
    text(f"Timestamp (us): {current_timestamp}", 10, 60) # Display timestamp for reference


# This function is used by Processing 4 for responsive window sizing
def settings():
    size(800, 400)
