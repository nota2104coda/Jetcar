#!/usr/bin/env python3
"""
Test script for VLM inference service.
Usage: python3 scripts/test_vlm_service.py <image_path> <question>
"""

import sys
import base64
import requests
from pathlib import Path
from PIL import Image

def test_vlm(image_path: str, question: str, server_url: str = "http://localhost:8000"):
    """
    Test VLM inference with an image and question.
    
    Args:
        image_path: Path to image file
        question: Question to ask about the image
        server_url: URL of the inference server
    """
    # Check if server is healthy
    try:
        health = requests.get(f"{server_url}/health", timeout=5)
        print(f"✓ Server health: {health.json()}")
    except Exception as e:
        print(f"✗ Server not reachable: {e}")
        return
    
    # Load and encode image
    try:
        image = Image.open(image_path)
        print(f"✓ Loaded image: {image_path} ({image.size})")
        
        # Convert to base64
        from io import BytesIO
        buffer = BytesIO()
        image.save(buffer, format="PNG")
        image_base64 = base64.b64encode(buffer.getvalue()).decode("utf-8")
    except Exception as e:
        print(f"✗ Failed to load image: {e}")
        return
    
    # Send inference request
    print(f"\n🤔 Question: {question}")
    print("⏳ Running inference...")
    
    try:
        response = requests.post(
            f"{server_url}/infer",
            json={
                "image_base64": image_base64,
                "prompt": question,
                "max_tokens": 256,
                "temperature": 0.7
            },
            timeout=60
        )
        
        result = response.json()
        
        if result.get("success"):
            print(f"\n✓ Response: {result['output_text']}")
            print(f"  Linear velocity: {result['linear_x']:.2f}")
            print(f"  Angular velocity: {result['angular_z']:.2f}")
        else:
            print(f"\n✗ Error: {result.get('error', 'Unknown error')}")
            
    except Exception as e:
        print(f"\n✗ Inference failed: {e}")


def main():
    if len(sys.argv) < 3:
        print("Usage: python3 test_vlm_service.py <image_path> <question>")
        print("\nExamples:")
        print("  python3 scripts/test_vlm_service.py test.jpg 'What do you see?'")
        print("  python3 scripts/test_vlm_service.py robot_view.png 'Is there an obstacle ahead?'")
        print("  python3 scripts/test_vlm_service.py scene.jpg 'Describe this image'")
        sys.exit(1)
    
    image_path = sys.argv[1]
    question = sys.argv[2]
    
    if not Path(image_path).exists():
        print(f"✗ Image not found: {image_path}")
        sys.exit(1)
    
    test_vlm(image_path, question)


if __name__ == "__main__":
    main()
