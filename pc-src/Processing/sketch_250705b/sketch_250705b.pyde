# Processing Python Mode - Sine Wave Plotter
# Reads sine wave values from Raspberry Pi Pico via serial

from processing.serial import Serial # Import the Serial library

myPort = None         # Serial object
BAUD_RATE = 115200    # Must match the Pico's baud rate

# Plotting variables
history = []          # List to store recent sine wave values
MAX_HISTORY_LENGTH = 500 # Number of points to keep in history
current_value = 0.0

# Define the expected range of your sine wave
# (Adjust these based on your Pico's AMPLITUDE and OFFSET values)
MIN_PLOT_VALUE = 0.0   # Corresponds to OFFSET - AMPLITUDE on Pico
MAX_PLOT_VALUE = 250.0 # Corresponds to OFFSET + AMPLITUDE on Pico (e.g., 127 + 100 = 227, round up for buffer)
# Remember, from the Pico code: AMPLITUDE = 100.0, OFFSET = 127.0
# So expected range is 127 - 100 = 27 to 127 + 100 = 227.
# We'll use 0-250 for plotting flexibility.

def setup():
    global myPort, history # Declare global variables
    size(800, 400) # Window size (width, height)
    background(20) # Dark background
    frameRate(60)  # Try to update at 60 frames per second

    print("Available serial ports:")
    print(Serial.list())

    # --- IMPORTANT: Configure your COM Port here! ---
    # From your error message, your Pico is likely on COM4, COM5, or COM6.
    # Let's assume your Pico is on COM5 based on your error output.
    port_name = "COM5" 
    # Or, if you want to be dynamic and assume the last one in the list is the Pico (a guess):
    # port_name = str(Serial.list()[-1]) # Convert Java String to Python string if needed

    try:
        myPort = Serial(this, port_name, BAUD_RATE)
        myPort.clear() # Clear any pending serial data
        # Corrected line: Use 10 (ASCII for newline) instead of '\n'
        myPort.bufferUntil(10) 
        print("Connected to serial port:", port_name)
    except Exception as e:
        print("Error opening serial port:", port_name)
        print("Ensure your Pico is connected and the COM port is correct.")
        print("Error details:", e)
        print("Available ports:", Serial.list())
        exit() # Exit the sketch if port cannot be opened

    # Initialize history list
    for _ in range(MAX_HISTORY_LENGTH):
        history.append(0.0) # Fill with zeros initially

def draw():
    global history, current_value # Declare global variables if modified

    background(20) # Clear the background each frame

    # Draw grid lines
    stroke(50)
    for i in range(0, width, 50):
        line(i, 0, i, height)
    for i in range(0, height, 50):
        line(0, i, width, i)

    # Draw plot area lines for reference
    stroke(100)
    # Line for the average (OFFSET from Pico)
    y_offset_line = map(127.0, MIN_PLOT_VALUE, MAX_PLOT_VALUE, height, 0)
    line(0, y_offset_line, width, y_offset_line)
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

    # Display current value
    fill(255)
    textSize(24)
    text("Current Value: %.2f" % current_value, 10, 30)

# This function is called when a serial event occurs (data received)
# This function is called when a serial event occurs (data received)
def serialEvent(p): # 'p' is the Serial object that triggered the event
    global history, current_value # Declare global variables if modified

    try:
        # Corrected line: Use 10 (ASCII for newline) instead of '\n'
        in_string = p.readStringUntil(10) 
        if in_string is not None:
            in_string = in_string.strip() # Remove leading/trailing whitespace

            # Convert the string to a float
            sine_value = float(in_string)

            # Store the current value for display
            current_value = sine_value

            # Add the new value to the history list
            history.append(sine_value)
            # Keep the history list at a fixed length
            if len(history) > MAX_HISTORY_LENGTH:
                history.pop(0) # Remove the oldest value
    except Exception as e:
        # Added a check for empty string after strip, which can cause ValueError for float()
        if "could not convert string to float" in str(e) and in_string == "":
            print("Received empty string, ignoring.")
        else:
            print("Error reading serial data:", e)
            # You might want to add more robust error handling or simply ignore bad data
            
# This function is used by Processing 4 for responsive window sizing
# If you are using Processing 3, you can remove this function,
# or simply put size(800, 400) directly in setup() as shown.
def settings():
    size(800, 400)
