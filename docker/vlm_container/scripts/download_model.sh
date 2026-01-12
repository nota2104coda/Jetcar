#!/bin/bash
# Downloads VLM model on first container startup

set -e

mkdir -p $HF_HOME
mkdir -p $TORCH_HOME

echo "Downloading model..."
python3 << 'EOF'
import os
from transformers import AutoTokenizer, AutoModel
from PIL import Image
import requests

os.environ['HF_HOME'] = os.getenv('HF_HOME', '/data/models/huggingface')
os.environ['TRANSFORMERS_CACHE'] = os.getenv('TRANSFORMERS_CACHE', '/data/models/huggingface')

# Pre-download the model
print("Downloading model weights...")
model_id = "Qwen/Qwen2.5-VL-3B-Instruct"
try:
    tokenizer = AutoTokenizer.from_pretrained(model_id)
    print(f"✓ Model {model_id} downloaded successfully")
except Exception as e:
    print(f"✗ Failed to download model: {e}")
    exit(1)

print(f"Models cached at: {os.environ['HF_HOME']}")
EOF

echo "Model download complete!"