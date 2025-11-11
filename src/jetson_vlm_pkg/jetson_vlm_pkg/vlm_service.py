#!$HOME/PicoWCar/.venv/bin/python

# This node will handle the VLA model's inference and publish motor commands.
#ROS2 node and QoS packages
import rclpy
from rclpy.node import Node
from rclpy.qos import QoSProfile, ReliabilityPolicy, HistoryPolicy, DurabilityPolicy
# Import standard ROS2 messages
from geometry_msgs.msg import Twist
from std_msgs.msg import String
# Imports for image handling
from sensor_msgs.msg import Image
from cv_bridge import CvBridge
import cv2 
import base64 # Needed for in-memory image encoding
import numpy as np # OpenCV images are NumPy arrays
# basic system packages
import os
import json
import time
# 're' module for pattern matching in LLM output cleaning
import re 
# LLM inference library
from llama_cpp import Llama

# Import your custom message
from pi5_pkg.msg import HMIButtons

LLM_MODEL_PATH="~/PicoWCar/libs/qwen2vl2b/tensorblock-qwen2-vl-2b-q4_K_M.gguf"
# Path to the GGUF model file (set in llm_setup_instructions.md)
MODEL_PATH = os.environ.get("LLM_MODEL_PATH")
if not MODEL_PATH:
    raise ValueError("LLM_MODEL_PATH environment variable not set. Please set it to the path of your GGUF file.")

# Absolute safety limits for Twist commands (HARD CLAMPING IN CODE)
MAX_LINEAR_VELOCITY = 0.25  # Maximum forward/backward speed (m/s)
MAX_ANGULAR_VELOCITY = 1.0 # Maximum rotation speed (rad/s)

# Control loop rate (1.0 Hz initially for raspberry pi 5)
INFERENCE_RATE_HZ = 1.0 


# The single prompt template the LLM will see
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

class VLADecisionNode(Node):
    """
    ROS 2 Node that uses a local GGUF LLM to convert natural language 
    instructions and camera images into standard geometry_msgs/Twist robot commands.
    It operates on a periodic timer to ensure a consistent output rate (1 Hz).
    """
    def __init__(self):
        super().__init__('vla_decision_node')
        self.get_logger().info("VLA Decision Node has started.")

        # Initialize CV Bridge for ROS/OpenCV image conversion
        self.bridge = CvBridge()
        # Stores the Base64 encoded image string
        self.latest_image_b64: str = None 
        self.latest_image_received = False
        # Placeholder for HMI state (using String for simplicity since HMIButtons is custom)
        # self.user_instruction: str = "" # Stores the last instruction received
        self.user_instruction: str = "Find a person's face in image and navigate so that the face fills 70 percent of screen height" # bespoke instruction
        self.latest_hmi_command = String() 

        #1 Create a QoS profile for reliable communication
        qos_profile_sensor = QoSProfile(
            reliability=ReliabilityPolicy.BEST_EFFORT,
            history=HistoryPolicy.KEEP_LAST,
            depth=1
        )
        qos_profile_control = QoSProfile(
            reliability=ReliabilityPolicy.RELIABLE,
            history=HistoryPolicy.KEEP_LAST,
            depth=1,
            durability=DurabilityPolicy.VOLATILE
        )

        # 2. Create a publisher for motor commands
        self.cmd_vel_publisher = self.create_publisher(Twist, '/cmd_vel', qos_profile_control)

        # Create subscribers
        self.image_subscriber = self.create_subscription(
            Image,
            'camera/image_raw',
            self.image_callback,
            qos_profile_sensor
        )

        self.hmi_subscriber = self.create_subscription(
            HMIButtons,
            'hmi_buttons',
            self.hmi_callback,
            qos_profile_sensor
        )
        # Subscriber for text prompts (just stores the instruction, doesn't trigger LLM)
        self.subscription = self.create_subscription(
            String,
            '/user_prompt',
            self.user_prompt_callback, # Renamed for clarity
            1 # Default QoS
        )
        # Timer for the main control loop at the desired rate
        self.inference_timer = self.create_timer(
            1.0 /  ,
            self.inference_loop
        )


       # 3. Initialize LLM (Ensure this is a Multimodal/LLaVA GGUF model)
        try:
            self.llm = Llama(
                model_path=MODEL_PATH,
                n_ctx=2048,           
                n_gpu_layers=0,       
                n_threads=os.cpu_count() or 4, # Use all available CPU cores
                verbose=False         
            )
            self.get_logger().info(f"LLM loaded successfully from {MODEL_PATH}")
            self.get_logger().info(f"Running on {self.llm.n_threads} CPU threads.")
            self.get_logger().info(f"Node ready. LLM control loop commissioned at {INFERENCE_RATE_HZ} Hz.")
        except Exception as e:
            self.get_logger().error(f"Failed to load LLM: {e}")
            raise

        
    def image_callback(self, msg: Image):
        """
        Callback executed when a new camera image is received.
        Converts the image to Base64 in memory for low-latency passing to the LLM.
        This runs at camera frequency and caches the latest frame.
        """
        try:
            # Convert ROS Image message to OpenCV image (NumPy array)
            cv_image: np.ndarray = self.bridge.imgmsg_to_cv2(msg, desired_encoding='bgr8')
            
            # Encode the image in memory as JPEG for LLM consumption
            encode_param = [int(cv2.IMWRITE_JPEG_QUALITY), 90]
            _, buffer = cv2.imencode('.jpg', cv_image, encode_param)
            
            # Convert the byte buffer to a Base64 string
            self.latest_image_b64 = base64.b64encode(buffer).decode('utf-8')
            
            if not self.latest_image_received:
                self.get_logger().info(f"First image received and Base64 encoded.")
                self.latest_image_received = True

        except Exception as e:
            self.get_logger().error(f"Failed to process and encode image: {e}")

    def hmi_callback(self, msg: String):
        """
        Callback for the HMI state command. Stores the latest state.
        (Note: Assumes String message for demonstration, replace with HMIButtons 
        and custom logic if using that message type).
        """
        self.user_instruction = msg.data
        self.get_logger().debug(f"Cached new instruction: '{self.user_instruction}'")

    def parse_llm_output(self, raw_output: str) -> Twist:
        """
        CRITICAL: Parses LLM's JSON output, converts types, and clamps 
        velocities to ensure robot safety.
        """
        twist = Twist()
        twist.linear.x = 0.0
        twist.angular.z = 0.0

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
            self.get_logger().error(f"Failed to parse or clamp LLM JSON output: {e}. Raw output: '{raw_output}'")
            # Return a safe, zero-velocity command on failure
            return twist 

    def inference_loop(self):
        """
        The periodic control loop (runs at INFERENCE_RATE_HZ ). 
        It executes the LLM inference using the latest cached data.
        """
        safe_twist = Twist() # Zero velocity for safe operation
        
        # --- SAFETY CHECK 1: Image Feed Status ---
        if not self.latest_image_received or not self.latest_image_b64:
            self.get_logger().warn("Image feed not ready. Publishing safe zero-velocity command.")
            self.publisher.publish(safe_twist) 
            return

        # --- SAFETY CHECK 2: User Instruction Status ---
        if not self.user_instruction.strip():
            # If no instruction is set, or it's just whitespace, do nothing (maintain safety)
            self.get_logger().debug("No active user instruction. Publishing safe zero-velocity command.")
            self.publisher.publish(safe_twist) 
            return


        user_instruction = self.user_instruction
        self.get_logger().info(f"1Hz Loop: Processing instruction: '{user_instruction}'")
        
        # 1. Create the structured multimodal chat prompt
        messages = [
            {"role": "system", "content": SYSTEM_INSTRUCTION_TEMPLATE},
            {"role": "user", "content": [
                # In-memory image data reference
                {"type": "image_data", "data": self.latest_image_b64},
                {"type": "text", "text": f"Complete the instruction: {user_instruction}"}
            ]}
        ]
        
        # 2. Perform Inference
        start_time = time.time()
        try:
            # Note: This heavy operation is now strictly limited by the 1Hz timer
            response = self.llm.create_chat_completion(
                messages=messages,
                max_tokens=60, 
                temperature=0.1, 
                stop=["\n"],     
            )
            
            raw_output = response['choices'][0]['message']['content']
            
            inference_time = time.time() - start_time
            self.get_logger().info(f"Inference complete in {inference_time:.2f}s. Raw output: {raw_output}")

            # 3. Parse and Publish the Twist command
            twist_command = self.parse_llm_output(raw_output)
            self.publisher.publish(twist_command)
            
            self.get_logger().info(f"Published Twist: linear_x={twist_command.linear.x:.2f}, angular_z={twist_command.angular.z:.2f}")

        except Exception as e:
            # --- SAFETY FALLBACK: Unhandled LLM Error ---
            self.get_logger().error(f"FATAL: Unhandled error during LLM inference: {e}. Publishing safe zero-velocity command.")
            self.publisher.publish(safe_twist)


def main(args=None):
    rclpy.init(args=args)
    node = None
    try:
        node = VLADecisionNode() 
        rclpy.spin(node)
    except ValueError as e: 
        print(f"Node setup error: {e}")
    except Exception as e:
        print(f"A fatal error occurred: {e}")
    finally:
        if node is not None:
            node.destroy_node()
        rclpy.shutdown()

if __name__ == '__main__':
    main()
