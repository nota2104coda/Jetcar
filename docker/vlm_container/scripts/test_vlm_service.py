#!/usr/bin/env python3
"""
Test script for VLM inference server.
Validates model loading, inference, and motion command parsing.
Run this BEFORE launching ROS2 nodes.
"""

import requests
import base64
import json
import time
import sys
from pathlib import Path
from PIL import Image
import numpy as np

# Config
API_URL = "http://localhost:8000/infer"
HEALTH_URL = "http://localhost:8000/health"
MODEL_INFO_URL = "http://localhost:8000/model-info"

def check_service_health():
    """Verify VLM service is running and healthy."""
    print("[*] Checking VLM service health...")
    try:
        response = requests.get(HEALTH_URL, timeout=5)
        if response.status_code == 200:
            print("✓ Service is healthy")
            info = requests.get(MODEL_INFO_URL, timeout=5).json()
            print(f"  Model: {info['model_id']}")
            print(f"  Device: {info['device']}")
            print(f"  Quantization: {info['quantization']}")
            return True
        else:
            print(f"✗ Health check failed: {response.status_code}")
            return False
    except Exception as e:
        print(f"✗ Cannot reach service: {e}")
        print(f"  Ensure container is running: docker ps | grep vlm")
        return False

def create_test_image(text="TEST", size=(480, 480)):
    """Create a simple test image."""
    img = Image.new('RGB', size, color='white')
    # Add a simple gradient pattern
    pixels = img.load()
    for i in range(size[0]):
        for j in range(size[1]):
            pixels[i, j] = (i % 256, j % 256, (i + j) % 256)
    return img

def test_inference(prompt="What do you see in this image? Respond with FORWARD or STOP."):
    """Test inference with a synthetic image."""
    print(f"\n[*] Testing inference with prompt: '{prompt}'")
    
    # Create test image
    test_img = create_test_image()
    
    # Encode to base64
    import io
    buffer = io.BytesIO()
    test_img.save(buffer, format='JPEG')
    image_b64 = base64.b64encode(buffer.getvalue()).decode('utf-8')
    
    # Make request
    payload = {
        "image_base64": image_b64,
        "prompt": prompt,
        "max_tokens": 256,
        "temperature": 0.5
    }
    
    try:
        print("  Sending inference request...")
        start = time.time()
        response = requests.post(API_URL, json=payload, timeout=30)
        elapsed = time.time() - start
        response.raise_for_status()
        
        result = response.json()
        print(f"✓ Inference succeeded ({elapsed:.1f}s)")
        print(f"  Output: {result['output_text'][:100]}...")
        print(f"  Motion: linear_x={result['linear_x']:.2f}, angular_z={result['angular_z']:.2f}")
        
        return True
    
    except requests.exceptions.Timeout:
        print("✗ Request timed out (model may be loading)")
        return False
    except Exception as e:
        print(f"✗ Inference failed: {e}")
        return False

def test_parsing():
    """Test command parsing."""
    print("\n[*] Testing command parsing...")
    
    test_cases = [
        ("Follow the ball FORWARD", 0.5, 0.0),
        ("Turn LEFT and go forward", 0.5, 0.5),
        ("Turn RIGHT now", 0.0, -0.5),
        ("STOP immediately", 0.0, 0.0),
    ]
    
    for text, expected_x, expected_z in test_cases:
        # Simulate parse_vlm_output logic
        text_upper = text.upper()
        linear_x = 0.0
        angular_z = 0.0
        
        if "FORWARD" in text_upper or "GO" in text_upper:
            linear_x = 0.5
        elif "STOP" in text_upper:
            linear_x = 0.0
        
        if "LEFT" in text_upper:
            angular_z = 0.5
        elif "RIGHT" in text_upper:
            angular_z = -0.5
        
        match = abs(linear_x - expected_x) < 0.01 and abs(angular_z - expected_z) < 0.01
        status = "✓" if match else "✗"
        print(f"  {status} '{text}' → vx={linear_x}, wz={angular_z}")

def main():
    print("="*60)
    print("VLM Inference Server Test Suite")
    print("="*60)
    
    # Check service
    if not check_service_health():
        print("\n[!] Service is not running.")
        print("    Start with: docker-compose -f docker/docker-compose.orinnano.yml up -d")
        sys.exit(1)
    
    # Test parsing
    test_parsing()
    
    # Test inference
    print("\n[*] Running inference tests...")
    print("    (First run may be slow as model loads)")
    
    prompts = [
        "What is in this image?",
        "Should I move FORWARD or STOP?",
        "Is there a red object?",
    ]
    
    for prompt in prompts:
        if not test_inference(prompt):
            print("    Retrying in 5 seconds...")
            time.sleep(5)
            test_inference(prompt)
    
    print("\n" + "="*60)
    print("✓ All tests completed!")
    print("="*60)

if __name__ == "__main__":
    main()
