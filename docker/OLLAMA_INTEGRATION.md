# Ollama Moondream Integration for PicoWCar

This document describes the Ollama Moondream integration for vision-language control on the Jetson Orin Nano.

## Overview

The system now uses **Ollama** with the **Moondream** vision model instead of the custom SmolVLM inference server. This provides:

- ✅ **Faster inference**: Moondream is optimized for speed
- ✅ **Simpler deployment**: Use dustynv's pre-built Ollama container
- ✅ **OpenAI-compatible API**: Standard chat completions endpoint
- ✅ **Better model management**: Ollama handles model caching and loading

## Architecture

```
┌─────────────────┐      ┌──────────────────┐      ┌─────────────┐
│  Camera Topic   │─────▶│  SmolVLA Node    │─────▶│  cmd_vel    │
│ /camera/image   │      │  (ROS2 Python)   │      │  (Twist)    │
└─────────────────┘      └──────────────────┘      └─────────────┘
                                   │
                                   │ HTTP POST
                                   ▼
                         ┌──────────────────┐
                         │ Ollama Container │
                         │  (Moondream VLM) │
                         │  Port 9000       │
                         └──────────────────┘
```

## Container Configuration

### Docker Run Command (Manual)

```bash
docker run -d \
  --name picowcar-ollama-service \
  --rm \
  --gpus all \
  -p 9000:9000 \
  -e OLLAMA_MODEL=moondream \
  -e OLLAMA_MODELS=/root/.ollama \
  -e OLLAMA_HOST=0.0.0.0:9000 \
  -e OLLAMA_CONTEXT_LEN=4096 \
  -e OLLAMA_LOGS=/root/.ollama/ollama.log \
  -e HF_HUB_CACHE=/root/.cache/huggingface \
  -v /mnt/nvme/cache/ollama:/root/.ollama \
  -v /mnt/nvme/cache:/root/.cache \
  dustynv/ollama:main-r36.4.0
```

### Docker Compose (Recommended)

```bash
cd docker
docker-compose -f docker-compose.orinnano.yml up -d
```

### Helper Script (Easiest)

```bash
cd docker
./start_ollama.sh
```

## API Usage

The Ollama container exposes an OpenAI-compatible chat completions API on port 9000.

### Example Request

```bash
curl http://localhost:9000/v1/chat/completions \
  -H "Content-Type: application/json" \
  -d '{
    "model": "moondream",
    "messages": [{
      "role": "user",
      "content": [
        {
          "type": "text",
          "text": "identify the bird in 1 word ONLY"
        },
        {
          "type": "image_url",
          "image_url": {
            "url": "data:image/jpeg;base64,'"$IMAGE_B64"'"
          }
        }
      ]
    }],
    "max_tokens": 300
  }'
```

### Example Response

```json
{
  "id": "chatcmpl-744",
  "object": "chat.completion",
  "created": 1763400327,
  "model": "moondream",
  "choices": [{
    "index": 0,
    "message": {
      "role": "assistant",
      "content": "A small brown and white sparrow is perched..."
    },
    "finish_reason": "stop"
  }],
  "usage": {
    "prompt_tokens": 744,
    "completion_tokens": 49,
    "total_tokens": 793
  }
}
```

## ROS2 Integration

The `smolvla_node.py` has been updated to:

1. **Launch Ollama container** on startup
2. **Convert camera images** to base64-encoded JPEG
3. **Call Ollama API** using OpenAI chat completions format
4. **Parse responses** to extract motion commands (FORWARD, LEFT, RIGHT, STOP)
5. **Publish cmd_vel** Twist messages for robot control

### Node Parameters

- `vlm_prompt`: Custom prompt for vision-language inference (default: motion control prompt)

### Environment Variables

- `VLM_IMAGE`: Docker image (default: `dustynv/ollama:main-r36.4.0`)
- `OLLAMA_MODEL`: Ollama model name (default: `moondream`)
- `VLM_MODELS_DIR`: Model cache directory (default: `/mnt/nvme/cache/ollama`)
- `HF_TOKEN`: HuggingFace token (optional)

## Testing

### Test Ollama API Directly

```bash
cd docker
python3 test_ollama_moondream.py test_sparrow.jpg
```

### Test with ROS2 Node

```bash
# Terminal 1: Start Ollama container
cd docker
./start_ollama.sh

# Terminal 2: Launch ROS2 node (if configured)
ros2 run python_pkg smolvla_node

# Terminal 3: Publish test image
ros2 topic pub /camera/image_raw sensor_msgs/msg/Image ...
```

## Model Options

You can change the model by setting `OLLAMA_MODEL`:

- `moondream`: Fast vision model (default, recommended)
- `gemma3:4b`: Gemma 3 4B parameter model
- `llava`: LLaVA vision model
- Others from [Ollama model library](https://ollama.ai/library)

Example:
```bash
export OLLAMA_MODEL=gemma3:4b
./start_ollama.sh
```

## Performance

### Jetson Orin Nano (8GB)

- **Model loading**: ~30-60 seconds (first run, downloads model)
- **Inference time**: ~2-5 seconds per image
- **Memory usage**: ~2-3GB GPU RAM
- **Cache size**: ~2-4GB disk space

## Troubleshooting

### Container won't start

```bash
# Check Docker and NVIDIA runtime
docker run --rm --gpus all nvidia/cuda:11.4.0-base-ubuntu20.04 nvidia-smi

# Check logs
docker logs picowcar-ollama-service
```

### API connection errors

```bash
# Verify Ollama is running
curl http://localhost:9000/api/version

# Check port mapping
docker ps | grep ollama
```

### Model not loading

```bash
# Enter container and check
docker exec -it picowcar-ollama-service bash
ls -lh /root/.ollama/

# Check disk space
df -h /mnt/nvme/cache
```

### Slow inference

- Ensure GPU is being used: `nvidia-smi` should show Ollama process
- Check temperature throttling: `tegrastats`
- Reduce `OLLAMA_CONTEXT_LEN` if memory constrained

## Migration from SmolVLM

The old SmolVLM implementation used:
- Custom FastAPI inference server
- HuggingFace Transformers
- Port 8000
- Custom response format

The new Ollama implementation:
- Uses Ollama's optimized runtime
- OpenAI-compatible API
- Port 9000
- Standard chat completions format

**No ROS2 code changes needed** - the node handles the API differences internally.

## References

- [Ollama Documentation](https://github.com/ollama/ollama)
- [dustynv Jetson Containers](https://github.com/dusty-nv/jetson-containers)
- [Moondream Model](https://github.com/vikhyat/moondream)
- [OpenAI Chat Completions API](https://platform.openai.com/docs/api-reference/chat)

---

**Last Updated**: 2025-11-17  
**Tested On**: Jetson Orin Nano, JetPack 6.2 (L4T 36.4.0)
