import cv2
import time

# --- Configuration ---
# 0 corresponds to /dev/video0
CAMERA_INDEX = 0
OUTPUT_FILE = 'opencv_snapshot.jpg'
# --- End Configuration ---


# 1. Initialize the video capture object
# cv2.CAP_V4L2 is an optional flag to explicitly use the V4L2 backend
cap = cv2.VideoCapture(CAMERA_INDEX, cv2.CAP_V4L2)

if not cap.isOpened():
    print(f" Error: Could not open video device at index {CAMERA_INDEX} (/dev/video0).")
    exit()

# 2. Set the resolution (Optional, but good practice. Use a supported one like 640x480)
# Check the v4l2-ctl output: 640x480 is universally supported by your camera
cap.set(cv2.CAP_PROP_FRAME_WIDTH, 640)
cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 480)

# 3. Allow camera to warm up and stabilize auto-exposure/gain
time.sleep(2) 
print("Camera initialized. Capturing frame...")

# 4. Capture a frame
# Read() returns a success flag (ret) and the actual frame (frame)
ret, frame = cap.read()

# 5. Check if the frame was captured successfully
if ret:
    # Save the captured frame to a JPEG file
    cv2.imwrite(OUTPUT_FILE, frame)
    print(f"✅ Success! Snapshot saved as {OUTPUT_FILE}")
else:
    print("❌ Error: Failed to capture frame from the camera.")

# 6. Release the camera resource
cap.release()