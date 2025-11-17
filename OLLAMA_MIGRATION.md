# Migration to Ollama Moondream - Change Summary

## Overview

Successfully migrated the PicoWCar VLM system from custom SmolVLM inference server to **Ollama with Moondream** model.

## Files Modified

### 1. `src/pi5_pkg/pi5_pkg/smolvla_node.py`

**Major Changes:**
- Updated API endpoint from `http://localhost:8000/infer` to `http://localhost:9000/v1/chat/completions`
- Changed to OpenAI-compatible chat completions format
- Updated container configuration for Ollama:
  - Port 8000 → 9000
  - Added Ollama-specific environment variables
  - Updated health check endpoint
  - Increased timeout (10s → 30s) for model loading
- Modified request payload structure:
  - Old: Custom `{image_base64, prompt, max_tokens, temperature}`
  - New: OpenAI chat format with messages array
- Updated response parsing:
  - Old: Custom `{success, output_text, linear_x, angular_z}`
  - New: Extract from `choices[0].message.content`
- Added `parse_motion_command()` method to extract motion from text
- Updated default prompt to be more directive

**Key Differences:**

```python
# OLD API Call
payload = {
    "image_base64": image_b64,
    "prompt": self.prompt,
    "max_tokens": 256,
    "temperature": 0.5
}
response = requests.post("http://localhost:8000/infer", json=payload)
result = response.json()
linear_x = result.get("linear_x", 0.0)
angular_z = result.get("angular_z", 0.0)

# NEW API Call (Ollama/OpenAI format)
payload = {
    "model": "moondream",
    "messages": [{
        "role": "user",
        "content": [
            {"type": "text", "text": self.prompt},
            {"type": "image_url", "image_url": {"url": f"data:image/jpeg;base64,{image_b64}"}}
        ]
    }],
    "max_tokens": 300
}
response = requests.post("http://localhost:9000/v1/chat/completions", json=payload)
result = response.json()
output_text = result["choices"][0]["message"]["content"]
linear_x, angular_z = self.parse_motion_command(output_text)
```

### 2. `docker/docker-compose.orinnano.yml`

**Major Changes:**
- Service name: `vlm-service` → `ollama-service`
- Container name: `picowcar-vlm-service` → `picowcar-ollama-service`
- Docker image: `picowcar/jetson-vlm:latest` → `dustynv/ollama:main-r36.4.0`
- Removed custom build configuration
- Port mapping: `8000:8000` → `9000:9000`
- Environment variables:
  - Removed: `VLM_MODEL`, `USE_QUANTIZATION`, `HF_HOME`, `TRANSFORMERS_CACHE`, `TORCH_HOME`
  - Added: `OLLAMA_MODEL`, `OLLAMA_MODELS`, `OLLAMA_HOST`, `OLLAMA_CONTEXT_LEN`, `OLLAMA_LOGS`, `HF_HUB_CACHE`
- Volume mounts:
  - `/mnt/nvme/cache:/data/models` → `/mnt/nvme/cache/ollama:/root/.ollama`
  - Added: `/mnt/nvme/cache:/root/.cache`
- Health check: `http://localhost:8000/health` → `http://localhost:9000/api/version`
- Start period: 60s → 120s (Ollama needs more time)
- Memory limit: 5G → 6G

### 3. `docker/Dockerfile.orinnano`

**Major Changes:**
- Base image: `dustynv/l4t-pytorch:r36.4.0` → `dustynv/ollama:main-r36.4.0`
- Removed Python dependency installation (transformers, fastapi, uvicorn, etc.)
- Removed custom inference server copy
- Simplified to use Ollama's built-in runtime
- Port: 8000 → 9000
- Updated health check endpoint
- Environment variables align with Ollama

**Note**: This Dockerfile is now optional since we can directly use `dustynv/ollama:main-r36.4.0` without customization.

## New Files Created

### 1. `docker/test_ollama_moondream.py`

Python test script to verify Ollama Moondream API integration:
- Reads test image
- Encodes to base64
- Sends OpenAI-compatible request
- Parses response
- Tests motion command extraction
- Returns success/failure

Usage:
```bash
cd docker
python3 test_ollama_moondream.py test_sparrow.jpg
```

### 2. `docker/start_ollama.sh`

Bash helper script to launch Ollama container:
- Checks for existing containers
- Creates cache directories
- Starts container with proper configuration
- Waits for service to be ready
- Provides usage examples

Usage:
```bash
cd docker
./start_ollama.sh
```

### 3. `docker/OLLAMA_INTEGRATION.md`

Comprehensive documentation covering:
- Architecture overview
- Container configuration
- API usage examples
- ROS2 integration details
- Testing procedures
- Model options
- Performance benchmarks
- Troubleshooting guide
- Migration notes

## Configuration Changes

### Environment Variables

**Before (SmolVLM):**
```bash
VLM_IMAGE=picowcar/jetson-vlm:latest
VLM_MODELS_DIR=/mnt/nvme/cache
VLM_MODEL=HuggingFaceTB/SmolVLM-Instruct
USE_QUANTIZATION=false
```

**After (Ollama):**
```bash
VLM_IMAGE=dustynv/ollama:main-r36.4.0
OLLAMA_MODEL=moondream
VLM_MODELS_DIR=/mnt/nvme/cache/ollama
HF_TOKEN=<optional>
```

### API Endpoints

| Function | Before | After |
|----------|--------|-------|
| Inference | `POST /infer` | `POST /v1/chat/completions` |
| Health | `GET /health` | `GET /api/version` |
| Port | 8000 | 9000 |
| Format | Custom JSON | OpenAI Compatible |

## Testing Procedure

1. **Start Ollama container:**
   ```bash
   cd docker
   ./start_ollama.sh
   ```

2. **Test API directly:**
   ```bash
   python3 test_ollama_moondream.py test_sparrow.jpg
   ```

3. **Launch ROS2 node:**
   ```bash
   ros2 run pi5_pkg smolvla_node
   ```

4. **Verify container is running:**
   ```bash
   docker ps | grep ollama
   curl http://localhost:9000/api/version
   ```

## Benefits of Migration

1. **Simpler Deployment**: No custom inference server to maintain
2. **Better Performance**: Ollama is optimized for inference
3. **Standard API**: OpenAI-compatible format is widely supported
4. **Model Flexibility**: Easy to switch between Ollama models
5. **Proven Stability**: dustynv containers are well-tested on Jetson
6. **Smaller Footprint**: No need to maintain custom Python dependencies

## Backward Compatibility

⚠️ **Breaking Changes:**
- Port changed from 8000 to 9000
- API endpoint format is different
- Container name changed
- Environment variables changed

**Migration Path:**
- Old containers will need to be stopped/removed
- Environment variables need to be updated
- ROS2 node handles API differences automatically

## Next Steps

1. Test on actual Jetson Orin Nano hardware
2. Verify GPU utilization with `nvidia-smi`
3. Benchmark inference latency
4. Test with different Ollama models (llava, gemma3:4b)
5. Integrate with full ROS2 navigation stack
6. Update deployment documentation

## Rollback Plan

If issues arise, revert to SmolVLM:
```bash
git checkout HEAD~1 -- src/pi5_pkg/pi5_pkg/smolvla_node.py
git checkout HEAD~1 -- docker/docker-compose.orinnano.yml
git checkout HEAD~1 -- docker/Dockerfile.orinnano
```

## References

- Working curl command from user testing
- [dustynv/ollama container](https://github.com/dusty-nv/jetson-containers)
- [Moondream model](https://github.com/vikhyat/moondream)
- [Ollama API docs](https://github.com/ollama/ollama/blob/main/docs/api.md)

---

**Migration Date**: 2025-11-17  
**Tested On**: Development environment  
**Status**: ✅ Code complete, ready for hardware testing
