#!/usr/bin/env python3
"""
SmolVLA Node: ROS2 node that manages Ollama container lifecycle and publishes navigation commands.

This node:
1. Starts the Ollama container with Moondream on launch
2. Subscribes to camera images and HMI commands
3. Calls the Ollama API for vision-language inference
4. Publishes Twist (cmd_vel) messages for robot navigation
5. Stops the container on shutdown

Prompt example: "identify the object and respond with FORWARD to follow, LEFT/RIGHT to turn, or STOP"
"""

import rclpy
from rclpy.node import Node
from rclpy.qos import QoSProfile, ReliabilityPolicy, HistoryPolicy

import subprocess
import time
import base64
import json
import os
import signal
from threading import Thread
from io import BytesIO

import requests
from geometry_msgs.msg import Twist
from sensor_msgs.msg import Image, CompressedImage
from cv_bridge import CvBridge

# ============================================================================
# Configuration
# ============================================================================

DOCKER_IMAGE = os.getenv("VLM_IMAGE", "dustynv/ollama:main-r36.4.0")
CONTAINER_NAME = "picowcar-ollama-service"
OLLAMA_MODEL = os.getenv("OLLAMA_MODEL", "moondream")
VLM_API_URL = "http://localhost:9000/v1/chat/completions"
VLM_HEALTH_URL = "http://localhost:9000/api/version"
MODEL_CACHE_DIR = os.getenv("VLM_MODELS_DIR", "/mnt/nvme/cache/ollama")
API_TIMEOUT = 30  # seconds - Moondream can take time on first inference

# ============================================================================
# Docker Container Manager
# ============================================================================

class ContainerManager:
    """Manages Ollama Docker container lifecycle."""
    
    def __init__(self, image, container_name, model_cache_dir, ollama_model, node_logger):
        self.image = image
        self.container_name = container_name
        self.model_cache_dir = model_cache_dir
        self.ollama_model = ollama_model
        self.logger = node_logger
        self.container_id = None
    
    def is_running(self):
        """Check if container is running."""
        try:
            result = subprocess.run(
                ["docker", "ps", "--filter", f"name={self.container_name}", "--format", "{{.ID}}"],
                capture_output=True,
                text=True,
                timeout=5
            )
            return bool(result.stdout.strip())
        except Exception as e:
            self.logger.warn(f"Failed to check container status: {e}")
            return False
    
    def start(self):
        """Start the Ollama container."""
        if self.is_running():
            self.logger.info(f"Container {self.container_name} already running.")
            return True
        
        self.logger.info(f"Starting Ollama container {self.container_name}...")
        
        # Ensure model cache directory exists
        os.makedirs(self.model_cache_dir, exist_ok=True)
        
        try:
            # Get HF_TOKEN from environment if available
            hf_token = os.getenv("HF_TOKEN", "")
            
            cmd = [
                "docker", "run",
                "--name", self.container_name,
                "--rm",
                "--gpus", "all",
                "-p", "9000:9000",
                "-e", f"OLLAMA_MODEL={self.ollama_model}",
                "-e", "OLLAMA_MODELS=/root/.ollama",
                "-e", "OLLAMA_HOST=0.0.0.0:9000",
                "-e", "OLLAMA_CONTEXT_LEN=4096",
                "-e", "OLLAMA_LOGS=/root/.ollama/ollama.log",
                "-e", "DOCKER_PULL=always",
                "-e", "HF_HUB_CACHE=/root/.cache/huggingface",
                "-v", f"{self.model_cache_dir}:/root/.ollama",
                "-v", "/mnt/nvme/cache:/root/.cache",
                self.image
            ]
            
            # Add HF_TOKEN if available
            if hf_token:
                cmd.insert(cmd.index(self.image), "-e")
                cmd.insert(cmd.index(self.image), f"HF_TOKEN={hf_token}")
            
            self.container_process = subprocess.Popen(
                cmd,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                preexec_fn=os.setsid  # Create new process group
            )
            
            # Wait for Ollama to be ready
            self.logger.info("Waiting for Ollama service to be ready...")
            max_retries = 60  # Ollama needs more time to load model
            for i in range(max_retries):
                time.sleep(2)
                try:
                    response = requests.get(VLM_HEALTH_URL, timeout=3)
                    if response.status_code == 200:
                        self.logger.info("✓ Ollama service is healthy!")
                        # Give extra time for model loading
                        time.sleep(5)
                        return True
                except:
                    if i % 10 == 0:
                        self.logger.info(f"  Retry {i+1}/{max_retries}...")
            
            self.logger.error("Ollama service failed to become healthy within timeout.")
            self.stop()
            return False
        
        except Exception as e:
            self.logger.error(f"Failed to start container: {e}")
            return False
    
    def stop(self):
        """Stop the Ollama container."""
        self.logger.info(f"Stopping Ollama container {self.container_name}...")
        
        try:
            subprocess.run(
                ["docker", "stop", self.container_name],
                timeout=10,
                capture_output=True
            )
            subprocess.run(
                ["docker", "rm", self.container_name],
                timeout=5,
                capture_output=True
            )
            self.logger.info("Container stopped.")
        except Exception as e:
            self.logger.warn(f"Error stopping container: {e}")

# ============================================================================
# SmolVLA Node
# ============================================================================

class SmolVLANode(Node):
    """ROS2 node for Vision-Language-based navigation using Ollama/Moondream."""
    
    def __init__(self):
        super().__init__('smolvla_node')
        self.get_logger().info("SmolVLA Node initializing...")
        
        # Container manager
        self.container_manager = ContainerManager(
            DOCKER_IMAGE,
            CONTAINER_NAME,
            MODEL_CACHE_DIR,
            OLLAMA_MODEL,
            self.get_logger()
        )
        
        # Start container
        if not self.container_manager.start():
            self.get_logger().error("Failed to start VLM container. Shutting down.")
            raise RuntimeError("Container startup failed")
        
        # QoS profile
        qos_profile = QoSProfile(
            reliability=ReliabilityPolicy.BEST_EFFORT,
            history=HistoryPolicy.KEEP_LAST,
            depth=1
        )
        
        # Publishers and subscribers
        self.cmd_vel_pub = self.create_publisher(Twist, 'cmd_vel', qos_profile)
        
        self.image_sub = self.create_subscription(
            Image,
            'camera/image_raw',
            self.image_callback,
            qos_profile
        )
        
        # State
        self.latest_image = None
        self.bridge = CvBridge()
        self.processing = False
        
        # Default prompt (can be overridden)
        self.prompt = self.declare_parameter(
            'vlm_prompt',
            'identify the object in 1 word ONLY and respond with FORWARD to follow, LEFT to turn left, RIGHT to turn right, or STOP'
        ).get_parameter_value().string_value
        
        # Inference timer
        self.timer = self.create_timer(0.5, self.inference_callback)
        
        self.get_logger().info("✓ SmolVLA Node ready!")
    
    def image_callback(self, msg):
        """Store latest image."""
        self.latest_image = msg
    
    def inference_callback(self):
        """Periodically call Ollama/Moondream inference and publish commands."""
        if self.processing or self.latest_image is None:
            return
        
        self.processing = True
        try:
            # Convert image to base64
            cv_image = self.bridge.imgmsg_to_cv2(self.latest_image, desired_encoding='rgb8')
            from PIL import Image as PILImage
            buffer = BytesIO()
            pil_img = PILImage.fromarray(cv_image)
            pil_img.save(buffer, format='JPEG')
            image_b64 = base64.b64encode(buffer.getvalue()).decode('utf-8')
            
            # Call Ollama API with OpenAI-compatible chat completions format
            payload = {
                "model": OLLAMA_MODEL,
                "messages": [{
                    "role": "user",
                    "content": [
                        {
                            "type": "text",
                            "text": self.prompt
                        },
                        {
                            "type": "image_url",
                            "image_url": {
                                "url": f"data:image/jpeg;base64,{image_b64}"
                            }
                        }
                    ]
                }],
                "max_tokens": 300
            }
            
            response = requests.post(VLM_API_URL, json=payload, timeout=API_TIMEOUT)
            response.raise_for_status()
            result = response.json()
            
            # Extract text from OpenAI-compatible response
            if "choices" in result and len(result["choices"]) > 0:
                output_text = result["choices"][0]["message"]["content"]
                
                # Parse output to determine motion
                linear_x, angular_z = self.parse_motion_command(output_text)
                
                # Publish Twist
                twist = Twist()
                twist.linear.x = linear_x
                twist.angular.z = angular_z
                self.cmd_vel_pub.publish(twist)
                
                self.get_logger().debug(
                    f"Output: {output_text[:50]}... | "
                    f"Twist: Vx={twist.linear.x:.2f}, Wz={twist.angular.z:.2f}"
                )
            else:
                self.get_logger().warn(f"Invalid response format: {result}")
        
        except requests.exceptions.ConnectionError:
            self.get_logger().error(f"Cannot connect to Ollama at {VLM_API_URL}")
        except Exception as e:
            self.get_logger().error(f"Inference error: {e}")
        finally:
            self.processing = False
    
    def parse_motion_command(self, text: str) -> tuple:
        """
        Parse VLM output and extract motion commands.
        
        Returns: (linear_x, angular_z)
        """
        text_upper = text.upper()
        
        linear_x = 0.0
        angular_z = 0.0
        
        if "FORWARD" in text_upper or "MOVE FORWARD" in text_upper or "FOLLOW" in text_upper:
            linear_x = 0.5
        elif "BACKWARD" in text_upper or "RETREAT" in text_upper:
            linear_x = -0.3
        elif "STOP" in text_upper or "HALT" in text_upper:
            linear_x = 0.0
            angular_z = 0.0
        
        if "LEFT" in text_upper or "TURN LEFT" in text_upper:
            angular_z = 0.5
        elif "RIGHT" in text_upper or "TURN RIGHT" in text_upper:
            angular_z = -0.5
        
        return linear_x, angular_z
    
    def destroy_node(self):
        """Cleanup: stop container."""
        self.get_logger().info("Shutting down SmolVLA Node...")
        self.container_manager.stop()
        super().destroy_node()

# ============================================================================
# Main
# ============================================================================

def main(args=None):
    rclpy.init(args=args)
    node = SmolVLANode()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()

if __name__ == '__main__':
    main()