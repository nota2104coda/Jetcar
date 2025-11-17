import requests
import base64
import json
import os
from nano_llm import NanoLLM

# --- Configuration ---
IMAGE_PATH = os.path.expanduser("~/Downloads/boardwalk.jpg")
# 1. CHANGE THE URL to the native Ollama endpoint
OLLAMA_API_URL = "http://localhost:11434/api/chat" 
MODEL_NAME = "moondream"
PROMPT = "What is in this image?"

# --- Encode the Local Image to Base64 ---
def encode_image_to_base64(filepath):
    """Opens a local image file and returns it as a Base64 encoded string."""
    try:
        with open(filepath, "rb") as image_file:
            binary_data = image_file.read()
            base64_string = base64.b64encode(binary_data).decode("utf-8")
            return base64_string
    except FileNotFoundError:
        print(f"Error: Image file not found at {filepath}")
        return None

# --- 2. Build the NATIVE OLLAMA Payload ---
def build_payload(base64_image):
    """Builds the JSON payload for the native Ollama API."""
    return {
        "model": MODEL_NAME,
        "stream": False,  # We want the full response at once
        "messages": [
            {
                "role": "user",
                "content": PROMPT,
                # The native API takes images as a list in the 'images' key
                "images": [base64_image] 
            }
        ]
    }

# --- 3. Run the Test ---
print(f"Encoding image from: {IMAGE_PATH}")
b64_image = encode_image_to_base64(IMAGE_PATH)

if b64_image:
    print("Image encoded. Sending request to native Ollama API...")
    payload = build_payload(b64_image)
    
    try:
        response = requests.post(OLLAMA_API_URL, json=payload, timeout=600) # Increased timeout
        response.raise_for_status() 
        
        print("\n--- Server Response ---")
        response_data = response.json()
        print(response_data)
        
        # Print just the text content
        if "message" in response_data and "content" in response_data["message"]:
            print("\n--- Model's Answer ---")
            print(response_data["message"]["content"])

    except requests.exceptions.ConnectionError:
        print(f"\nError: Could not connect to {OLLAMA_API_URL}.")
    except requests.exceptions.Timeout:
        print("\nError: The request timed out. The model is still loading.")
    except requests.exceptions.RequestException as e:
        print(f"\nAn error occurred: {e}")
