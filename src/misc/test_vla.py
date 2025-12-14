#!$HOME/PicoWCar/.venv/bin/python

import os
import json
import time
import re 
import base64
import numpy as np
import cv2 

# --- Dependencies ---
# You need llama-cpp-python, numpy, and opencv-python installed:
# pip install llama-cpp-python opencv-python numpy
from llama_cpp import Llama

# --- Configuration (Copied from ROS Node) ---

# !!! CHANGE THIS TO YOUR QWEN2 GGUF MODEL PATH !!!
MODEL_PATH = "~/PicoWCar/libs/qwen2vl2b/tensorblock-qwen2-vl-2b-q4_K_M.gguf" 
# Ensure this path is resolved correctly for the Llama() call
MODEL_PATH = os.path.expanduser(MODEL_PATH) 

# !!! CHANGE THIS TO THE PATH OF YOUR TEST IMAGE !!!
IMAGE_PATH = "/home/jeevan/Downloads/trump.jpg"
USER_INSTRUCTION = "Find a person's face in image and navigate so that the face fills 70 percent of screen height and is horizontally centered"

# Safety limits (for output clamping)
MAX_LINEAR_VELOCITY = 1.0
MAX_ANGULAR_VELOCITY = 2.0

# The single prompt template (Must match ROS node exactly)
SYSTEM_INSTRUCTION_TEMPLATE = f"""
You are a reliable, JSON-only command generator for a mobile robot.
Your task is to convert a user's natural language instruction, using the visual context from the camera image provided with this prompt, into a single, valid 'geometry_msgs/Twist' command.
You MUST output a JSON object with only two keys: 'linear_x' (for forward/backward motion) and 'angular_z' (for turning).

Rules for Linear Velocity (forward/backward):
- The value for 'linear_x' must be between -{MAX_LINEAR_VELOCITY} (max backward) and {MAX_LINEAR_VELOCITY} (max forward).
- If the instruction is 'forward', use a positive value (e.g., 0.25 or 0.5).
- If the instruction is 'backward', use a negative value (e.g., -0.25 or -0.5).
- If the instruction is 'stop' or involves turning in place, use 0.0.

Rules for Angular Velocity (left/right turning):
- The value for 'angular_z' must be between -{MAX_ANGULAR_VELOCITY} (max right) and {MAX_ANGULAR_VELOCITY} (max left).
- If the instruction is 'turn left', use a positive value (e.g., 0.5 or 1.0).
- If the instruction is 'turn right', use a negative value (e.g., -0.5 or -1.0).
- If the instruction is 'go straight' or 'stop', use 0.0.

Rules for Speed:
- For 'speed 1', use 0.25 for linear movement and 0.5 for angular movement.
- For 'speed 2', use 0.5 for linear movement and 1.0 for angular movement.
- For 'stop', use 0.0 for both.

Example 1: User says "Move slowly toward the green object" -> {{"linear_x": 0.25, "angular_z": 0.0}}
Example 2: User says "Spin quickly right to face the obstacle" -> {{"linear_x": 0.0, "angular_z": -1.0}}
Example 3: User says "Stop the car immediately" -> {{"linear_x": 0.0, "angular_z": 0.0}}

Your response MUST be the raw, valid JSON object only. Do not add any extra text, explanations, or code blocks.
"""

# --- Utility Functions ---

class TwistMock:
    """Mock class for geometry_msgs/Twist for standalone testing."""
    def __init__(self, linear_x=0.0, angular_z=0.0):
        self.linear = type('Linear', (object,), {'x': linear_x})
        self.angular = type('Angular', (object,), {'z': angular_z})
    
    def __repr__(self):
        return f"Twist(linear.x={self.linear.x:.2f}, angular.z={self.angular.z:.2f})"


def parse_llm_output(raw_output: str) -> TwistMock:
    """
    Parses LLM's JSON output, converts types, and clamps velocities.
    (Matches logic from the ROS 2 node's parse_llm_output method)
    """
    twist = TwistMock()

    try:
        # 1. Clean the output (removes surrounding markdown/text)
        match = re.search(r'\{.*\}', raw_output, re.DOTALL)
        if not match:
            raise ValueError("Could not find a valid JSON object in the LLM output.")
        
        json_string = match.group(0)
        
        # 2. Parse the JSON
        data = json.loads(json_string)

        # 3. Validate and set Twist values
        linear_x = float(data.get('linear_x', 0.0))
        angular_z = float(data.get('angular_z', 0.0))
        
        # Hard clamping to safe velocity limits
        twist.linear.x = max(-MAX_LINEAR_VELOCITY, min(MAX_LINEAR_VELOCITY, linear_x))
        twist.angular.z = max(-MAX_ANGULAR_VELOCITY, min(MAX_ANGULAR_VELOCITY, angular_z))
        
        return twist

    except (json.JSONDecodeError, ValueError, TypeError) as e:
        print(f"ERROR: Failed to parse or clamp LLM JSON output: {e}")
        print(f"RAW OUTPUT: '{raw_output}'")
        # Return a safe, zero-velocity command on failure
        return TwistMock() 

def encode_image_to_base64(image_path: str) -> str:
    """Loads a JPEG image from disk and returns its Base64 encoding."""
    if not os.path.exists(image_path):
        raise FileNotFoundError(f"Image not found at: {image_path}")

    # Load image using OpenCV
    cv_image = cv2.imread(image_path)
    if cv_image is None:
        raise IOError(f"Failed to load image with OpenCV: {image_path}")
    
    # Encode the image in memory as JPEG
    # We use a JPEG format here as it's efficient for VLA models
    encode_param = [int(cv2.IMWRITE_JPEG_QUALITY), 90]
    _, buffer = cv2.imencode('.jpg', cv_image, encode_param)
    
    # Convert the byte buffer to a Base64 string
    return base64.b64encode(buffer).decode('utf-8')

def run_inference():
    """Initializes LLM and runs a single inference test."""
    print("--- LLM Inference Test Script ---")

    # 1. Load LLM
    try:
        if not os.path.exists(MODEL_PATH):
             print(f"FATAL: Model not found at {MODEL_PATH}")
             return
             
        llm = Llama(
            model_path=MODEL_PATH,
            n_ctx=2048,           
            n_gpu_layers=0,       
            n_threads=os.cpu_count() or 4, 
            verbose=False         
        )
        print(f"Model loaded: {MODEL_PATH}")
        # Note: Corrected log line for clarity (though not the error source)
        print(f"Threads: {llm.n_threads}, Context: {llm.n_ctx}") 
    except Exception as e:
        print(f"FATAL: Failed to load LLM: {e}")
        return

    # 2. Encode Image
    try:
        start_time = time.time()
        image_b64 = encode_image_to_base64(IMAGE_PATH)
        encoding_time = time.time() - start_time
        print(f"Image '{os.path.basename(IMAGE_PATH)}' encoded in {encoding_time:.4f}s.")
    except Exception as e:
        print(f"FATAL: Image processing failed: {e}")
        return
    
    # 3. Build Multimodal Prompt - **FIXED: Consolidated instruction into a single user message**
    
    # Consolidate System and User Instruction text
    full_instruction_text = SYSTEM_INSTRUCTION_TEMPLATE + \
        f"\n\nUSER INSTRUCTION: Complete the instruction using the image: {USER_INSTRUCTION}"

    messages = [
        # Place all content (image and combined text) under a single 'user' role
        # This simplifies the template and avoids AssertionErrors related to
        # template mismatches in certain GGUF builds.
        {"role": "user", "content": [
            {
                "type": "image_data", 
                "data": image_b64,
                "mime_type": "image/jpeg" 
            },
            {"type": "text", "text": full_instruction_text}
        ]}
    ]

    # 4. Perform Inference and Time
    print(f"\n--- Running Inference for: '{USER_INSTRUCTION}' ---")
    start_time = time.time()
    try:
        response = llm.create_chat_completion(
            messages=messages,
            max_tokens=60, 
            temperature=0.1, 
            stop=["\n"],     
        )
        
        end_time = time.time()
        
        raw_output = response['choices'][0]['message']['content']
        inference_time = end_time - start_time
        
        # 5. Parse Output
        twist_command = parse_llm_output(raw_output)

        # 6. Display Results
        print("-" * 50)
        print(f"Inference Time: {inference_time:.4f} seconds")
        print(f"Raw LLM Output: {raw_output}")
        print(f"Parsed Command: {twist_command}")
        print("-" * 50)


    except Exception as e:
        # **FIXED: Improved error reporting to capture the specific exception**
        print(f"\nFATAL: Unhandled error during LLM inference: {type(e).__name__}: {e}")
        print("-" * 50)

if __name__ == '__main__':
    run_inference()
