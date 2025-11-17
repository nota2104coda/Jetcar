#!/usr/bin/env python3
"""
Test script for Ollama Moondream API
Tests the OpenAI-compatible chat completions endpoint
"""

import base64
import json
import requests
import sys
from pathlib import Path

def test_ollama_moondream(image_path: str, prompt: str = "identify the bird in 1 word ONLY"):
    """Test Ollama Moondream vision API"""
    
    # Read and encode image
    with open(image_path, 'rb') as f:
        image_b64 = base64.b64encode(f.read()).decode('utf-8')
    
    # Prepare request
    url = "http://0.0.0.0:9000/v1/chat/completions"
    payload = {
        "model": "moondream",
        "messages": [{
            "role": "user",
            "content": [
                {
                    "type": "text",
                    "text": prompt
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
    
    print(f"Testing Ollama Moondream API...")
    print(f"Image: {image_path}")
    print(f"Prompt: {prompt}")
    print(f"URL: {url}")
    print("-" * 60)
    
    try:
        response = requests.post(url, json=payload, headers={"Content-Type": "application/json"}, timeout=30)
        response.raise_for_status()
        
        result = response.json()
        
        # Print full response
        print("Full Response:")
        print(json.dumps(result, indent=2))
        print("-" * 60)
        
        # Extract and print content
        if "choices" in result and len(result["choices"]) > 0:
            content = result["choices"][0]["message"]["content"]
            print(f"\nExtracted Content:\n{content}")
            print("-" * 60)
            
            # Test motion parsing
            print("\nMotion Command Parsing:")
            text_upper = content.upper()
            linear_x = 0.0
            angular_z = 0.0
            
            if "FORWARD" in text_upper or "FOLLOW" in text_upper:
                linear_x = 0.5
            elif "STOP" in text_upper:
                linear_x = 0.0
            
            if "LEFT" in text_upper:
                angular_z = 0.5
            elif "RIGHT" in text_upper:
                angular_z = -0.5
            
            print(f"  Linear X: {linear_x}")
            print(f"  Angular Z: {angular_z}")
            
            return True
        else:
            print("ERROR: Invalid response format")
            return False
            
    except requests.exceptions.ConnectionError:
        print("ERROR: Cannot connect to Ollama service at http://0.0.0.0:9000")
        print("Make sure the container is running with:")
        print("  docker ps | grep ollama")
        return False
    except Exception as e:
        print(f"ERROR: {e}")
        return False

if __name__ == "__main__":
    # Use test image if available
    test_image = Path(__file__).parent / "test_sparrow.jpg"
    
    if len(sys.argv) > 1:
        test_image = sys.argv[1]
    
    if not Path(test_image).exists():
        print(f"ERROR: Image not found: {test_image}")
        sys.exit(1)
    
    # Test with bird identification
    success = test_ollama_moondream(
        str(test_image),
        "identify the object in 1 word ONLY and respond with FORWARD to follow, LEFT to turn left, RIGHT to turn right, or STOP"
    )
    
    sys.exit(0 if success else 1)
