#!/usr/bin/env python3
"""
FastAPI inference server for Vision Language Models on Jetson Orin Nano.
Serves Qwen2.5-VL-3B-Instruct or SmolVLM with 4-bit quantization.
Communicates with ROS2 node via HTTP REST API.
"""

import os
import logging
import base64
from io import BytesIO
from pathlib import Path

import torch
import numpy as np
from PIL import Image
from fastapi import FastAPI, HTTPException
from pydantic import BaseModel
import uvicorn

# Model loading
from transformers import (
    AutoProcessor,
    AutoModelForVision2Seq,
    BitsAndBytesConfig,
)

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

# ============================================================================
# Configuration
# ============================================================================

MODEL_ID = os.getenv("VLM_MODEL", "Qwen/Qwen2.5-VL-3B-Instruct")
CACHE_DIR = os.getenv("HF_HOME", "/data/models")
DEVICE = "cuda" if torch.cuda.is_available() else "cpu"
QUANT_MODE = os.getenv("USE_QUANTIZATION", "false").lower()  # "false", "fp8", "bnb4", "bnb8", "prequant", "awq"

logger.info(f"Model: {MODEL_ID}")
logger.info(f"Cache: {CACHE_DIR}")
logger.info(f"Device: {DEVICE}")
logger.info(f"Quantization: {QUANT_MODE}")

# ============================================================================
# Global Model State (lazy loaded)
# ============================================================================

model = None
processor = None

# ============================================================================
# Request/Response Models
# ============================================================================

class InferenceRequest(BaseModel):
    """Request body for VLM inference."""
    image_base64: str  # Base64-encoded image
    prompt: str
    max_tokens: int = 256
    temperature: float = 0.7

class InferenceResponse(BaseModel):
    """Response from VLM inference."""
    success: bool
    output_text: str
    linear_x: float = 0.0
    angular_z: float = 0.0
    error: str = None

# ============================================================================
# Model Loading
# ============================================================================

def load_model():
    """Lazy load VLM model with optional quantization."""
    global model, processor
    
    if model is not None:
        logger.info("Model already loaded.")
        return
    
    logger.info(f"Loading {MODEL_ID}...")
    
    try:
        processor = AutoProcessor.from_pretrained(
            MODEL_ID,
            cache_dir=CACHE_DIR,
            trust_remote_code=True
        )
        
        # Memory config for Jetson Orin Nano (8GB VRAM)
        # SmolVLM-Instruct 2B: ~3.5GB in FP16
        max_memory = {0: "3500MiB", "cpu": "30GiB"}
        
        model_loaded = False
        
        # AWQ quantization (GPU-optimized 4-bit)
        if QUANT_MODE == "awq":
            try:
                logger.info("Loading AWQ quantized model...")
                model = AutoModelForVision2Seq.from_pretrained(
                    MODEL_ID,
                    device_map="auto",
                    max_memory=max_memory,
                    cache_dir=CACHE_DIR,
                    trust_remote_code=True,
                    low_cpu_mem_usage=True,
                )
                logger.info("✓ Loaded AWQ quantized model (~2GB VRAM)")
                model_loaded = True
            except Exception as awq_err:
                logger.warning(f"AWQ load failed: {awq_err}")
        
        # Pre-quantized model (e.g., unsloth 4-bit)
        if QUANT_MODE == "prequant":
            try:
                logger.info("Loading pre-quantized model (no runtime quantization)...")
                model = AutoModelForVision2Seq.from_pretrained(
                    MODEL_ID,
                    device_map="auto",
                    max_memory=max_memory,
                    cache_dir=CACHE_DIR,
                    trust_remote_code=True,
                    low_cpu_mem_usage=True,
                    offload_state_dict=True,
                )
                logger.info("✓ Loaded pre-quantized model (~2GB VRAM)")
                model_loaded = True
            except Exception as prequant_err:
                logger.warning(f"Pre-quantized load failed: {prequant_err}")
        
        # FP8 quantization (native CUDA support on Orin Nano)
        if QUANT_MODE == "fp8":
            try:
                logger.info("Loading with FP8 quantization (native CUDA)...")
                model = AutoModelForVision2Seq.from_pretrained(
                    MODEL_ID,
                    torch_dtype=torch.float8_e4m3fn,
                    device_map="auto",
                    max_memory=max_memory,
                    cache_dir=CACHE_DIR,
                    trust_remote_code=True,
                    low_cpu_mem_usage=True,
                )
                logger.info("✓ Loaded with FP8 quantization (~3GB VRAM)")
                model_loaded = True
            except Exception as fp8_err:
                logger.warning(f"FP8 failed: {fp8_err}, trying FP16 fallback...")
        
        # BNB 4-bit quantization
        elif QUANT_MODE == "bnb4":
            try:
                logger.info("Loading with BNB 4-bit quantization...")
                from transformers import BitsAndBytesConfig
                
                bnb_config = BitsAndBytesConfig(
                    load_in_4bit=True,
                    bnb_4bit_use_double_quant=True,
                    bnb_4bit_quant_type="nf4",
                    bnb_4bit_compute_dtype=torch.float16,
                )
                
                model = AutoModelForVision2Seq.from_pretrained(
                    MODEL_ID,
                    quantization_config=bnb_config,
                    device_map="auto",
                    max_memory=max_memory,
                    cache_dir=CACHE_DIR,
                    trust_remote_code=True,
                    low_cpu_mem_usage=True,
                )
                logger.info("✓ Loaded with BNB 4-bit")
                model_loaded = True
            except Exception as bnb_err:
                logger.warning(f"BNB 4-bit failed: {bnb_err}")
        
        # BNB 8-bit quantization
        elif QUANT_MODE == "bnb8":
            try:
                logger.info("Loading with BNB 8-bit quantization...")
                model = AutoModelForVision2Seq.from_pretrained(
                    MODEL_ID,
                    load_in_8bit=True,
                    device_map="auto",
                    max_memory=max_memory,
                    cache_dir=CACHE_DIR,
                    trust_remote_code=True,
                    low_cpu_mem_usage=True,
                )
                logger.info("✓ Loaded with BNB 8-bit")
                model_loaded = True
            except Exception as q8_err:
                logger.warning(f"BNB 8-bit failed: {q8_err}")
        
        # Fallback: FP16 (standard)
        if not model_loaded:
            logger.info("Loading standard FP16 model...")
            model = AutoModelForVision2Seq.from_pretrained(
                MODEL_ID,
                torch_dtype=torch.float16,
                device_map="auto",
                max_memory=max_memory,
                cache_dir=CACHE_DIR,
                trust_remote_code=True,
                low_cpu_mem_usage=True,
                offload_state_dict=True,  # Offload to CPU during loading
            )
            logger.info("✓ Loaded with FP16 (~6GB VRAM)")
        
        logger.info("✓ Model loaded and ready!")
        
    except Exception as e:
        logger.error(f"Failed to load model: {e}")
        raise

# ============================================================================
# Inference Logic
# ============================================================================

def run_inference(image: Image.Image, prompt: str, max_tokens: int, temperature: float):
    """Execute VLM inference."""
    global model, processor
    
    if model is None or processor is None:
        load_model()
    
    try:
        # Prepare inputs
        inputs = processor(
            images=image,
            text=prompt,
            return_tensors="pt",
            padding=True,
        ).to(DEVICE)
        
        # Generate
        with torch.no_grad():
            output_ids = model.generate(
                **inputs,
                max_new_tokens=max_tokens,
                temperature=temperature,
                do_sample=True if temperature > 0 else False,
            )
        
        # Decode
        generated_text = processor.batch_decode(output_ids, skip_special_tokens=True)[0]
        
        return generated_text
    
    except Exception as e:
        logger.error(f"Inference error: {e}")
        raise

def parse_vlm_output(text: str) -> tuple:
    """
    Parse VLM output and extract motion commands.
    
    Returns: (output_text, linear_x, angular_z)
    """
    text_upper = text.upper()
    
    # Simple command parsing
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
    
    return text, linear_x, angular_z

# ============================================================================
# FastAPI App
# ============================================================================

app = FastAPI(title="VLM Inference Server", version="1.0")

@app.get("/health")
async def health_check():
    """Health check endpoint."""
    return {"status": "ok", "model": MODEL_ID}

@app.post("/infer", response_model=InferenceResponse)
async def infer(request: InferenceRequest):
    """Run VLM inference and return motion commands."""
    try:
        # Decode image
        image_data = base64.b64decode(request.image_base64)
        image = Image.open(BytesIO(image_data))
        
        # Run inference
        output_text = run_inference(
            image,
            request.prompt,
            request.max_tokens,
            request.temperature
        )
        
        # Parse output
        text, linear_x, angular_z = parse_vlm_output(output_text)
        
        return InferenceResponse(
            success=True,
            output_text=text,
            linear_x=linear_x,
            angular_z=angular_z,
        )
    
    except Exception as e:
        logger.error(f"Inference request failed: {e}")
        return InferenceResponse(
            success=False,
            output_text="",
            error=str(e)
        )

@app.get("/model-info")
async def model_info():
    """Get model information."""
    return {
        "model_id": MODEL_ID,
        "cache_dir": CACHE_DIR,
        "device": DEVICE,
        "quantization": QUANT_MODE,
        "model_loaded": model is not None,
    }

# ============================================================================
# Main
# ============================================================================

if __name__ == "__main__":
    # Pre-load model on startup (optional, can be lazy)
    logger.info("Starting VLM Inference Server...")
    
    # Load model now to catch issues early
    load_model()
    
    # Start server
    uvicorn.run(
        app,
        host="0.0.0.0",
        port=8000,
        log_level="info"
    )
