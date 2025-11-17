# SmolVLA + Qwen2.5-VL Integration Guide

## Architecture

```
Jetson Orin Nano (L4T 36.4.7, JetPack 6.2)
├── ROS2 Humble (Host)
│   └── smolvla_node.py
│       ├── Manages container lifecycle (start/stop)
│       ├── Subscribes to /camera/image_raw
│       └── Publishes /cmd_vel (Twist)
│
└── Docker Container
    ├── Ubuntu 22.04 base
    ├── CUDA 12.2 (from nvcr.io/nvidia/l4t-pytorch:r36.2.0)
    ├── FastAPI inference server (port 8000)
    ├── Qwen2.5-VL-3B-Instruct model (4-bit BNB quantized)
    └── Models cached at /mnt/nvme/cache (persisted)
```

## Prerequisites

1. **Jetson Requirements:**
   - JetPack 6.2 or later
   - L4T 36.4.7+
   - Docker installed and user in docker group
   - At least 8GB free disk space (for model cache)
   - GPU must have at least 6GB VRAM

2. **Host Requirements:**
   ```bash
   # Add user to docker group (avoid sudo)
   sudo usermod -aG docker $USER
   newgrp docker
   
   # Verify
   docker ps
   ```

3. **Model Cache Directory:**
   ```bash
   # Create persistent model storage
   sudo mkdir -p /mnt/nvme/cache
   sudo chmod 755 /mnt/nvme/cache
   
   # Verify
   ls -la /mnt/nvme/cache
   ```

## Setup Steps

### Step 1: Build Docker Image

From the repo root:

```bash
cd /home/jeevan/PicoWCar

# Build the image (first time ~10-15 min)
docker-compose -f docker/docker-compose.orinnano.yml build

# Verify
docker images | grep picowcar/jetson-vlm
```

### Step 2: Download Model (First Time Only)

This happens automatically on first container start:

```bash
# Start the service (will download model to /mnt/nvme/cache)
docker-compose -f docker/docker-compose.orinnano.yml up -d

# Monitor progress
docker logs -f picowcar-vlm-service

# Wait for "✓ Model loaded and ready!" message
# Then check
docker ps | grep vlm  # Should be running
```

First download takes **5-10 minutes** depending on internet speed. Subsequent starts are instant (cached).

### Step 3: Test VLM Service

```bash
# From repo root
python3 docker/vlm_container/scripts/test_vlm_service.py

# Expected output:
# ✓ Service is healthy
# ✓ Inference succeeded
# ✓ Motion commands parsed correctly
```

### Step 4: Build ROS2 Package

```bash
cd /home/jeevan/PicoWCar

# Build pi5_pkg (if not already built)
source /opt/ros/humble/setup.bash
colcon build --packages-select pi5_pkg

# Verify executable
ls -la install/pi5_pkg/lib/pi5_pkg/smolvla_node
```

### Step 5: Create ROS2 Launch File

If you don't have one, create `src/pi5_pkg/launch/vlm_navigation.launch.py`:

```python
from launch import LaunchDescription
from launch_ros.actions import Node

def generate_launch_description():
    return LaunchDescription([
        Node(
            package='pi5_pkg',
            executable='smolvla_node',
            name='smolvla_decision',
            output='screen',
            parameters=[{
                'vlm_prompt': 'Follow the red ball. Respond with FORWARD, LEFT, RIGHT, or STOP.'
            }],
        ),
        # Add your camera node here if needed
    ])
```

### Step 6: Launch ROS2 with VLM

```bash
# Terminal 1: Source ROS
cd /home/jeevan/PicoWCar
source /opt/ros/humble/setup.bash
source install/setup.bash

# Launch (starts container automatically)
ros2 launch pi5_pkg vlm_navigation.launch.py

# Expected output:
# [smolvla_node-1] Starting VLM container...
# [smolvla_node-1] Waiting for VLM service to be ready...
# [smolvla_node-1] ✓ VLM service is healthy!
# [smolvla_node-1] ✓ SmolVLA Node ready!
```

## Using the Node

### Parameters

Set via `ros2 launch`:

```bash
ros2 launch pi5_pkg vlm_navigation.launch.py vlm_prompt:="Find and follow the red cone"
```

Or in launch file:

```python
Node(
    package='pi5_pkg',
    executable='smolvla_node',
    name='smolvla_decision',
    parameters=[{
        'vlm_prompt': 'Your custom instruction here'
    }]
)
```

### Topics

- **Subscribed:**
  - `/camera/image_raw` (sensor_msgs/Image) - camera feed
  
- **Published:**
  - `/cmd_vel` (geometry_msgs/Twist) - navigation commands
    - `linear.x` - forward/backward velocity (-1.0 to 1.0)
    - `angular.z` - rotation velocity (-1.0 to 1.0)

### Example: Echo Commands

```bash
# Terminal 2: Monitor published commands
ros2 topic echo /cmd_vel
```

## Troubleshooting

### Service Won't Start

```bash
# Check container logs
docker logs picowcar-vlm-service

# If port 8000 is in use
lsof -i :8000
docker ps -a  # Find conflicting container
```

### Model Download Fails

```bash
# Check disk space
df -h /mnt/nvme/cache

# Check internet connectivity
curl https://huggingface.co

# Manually download (from another machine with more bandwidth)
docker run --rm -v /mnt/nvme/cache:/data/models \
  picowcar/jetson-vlm:latest python3 -c \
  "from transformers import AutoModel; AutoModel.from_pretrained('Qwen/Qwen2.5-VL-3B-Instruct')"
```

### Inference is Slow

- **First inference:** ~10-20s (model warming up)
- **Subsequent:** ~2-3s (expected)
- If slower: check GPU usage (`nvidia-smi`), reduce image size in node

### Out of Memory

- Model needs ~5-6GB VRAM in 4-bit mode
- Verify with: `nvidia-smi`
- Reduce `max_tokens` in inference request (currently 256)

### Container Keeps Restarting

```bash
# Check logs
docker logs --tail 50 picowcar-vlm-service

# Common causes:
# - CUDA version mismatch: verify nvidia-docker is installed
# - Out of memory: reduce `memory` in docker-compose
# - Model download incomplete: clear /mnt/nvme/cache and retry
```

## Manual Container Management

```bash
# Start service manually
docker-compose -f docker/docker-compose.orinnano.yml up -d

# View logs
docker logs -f picowcar-vlm-service

# Stop service
docker-compose -f docker/docker-compose.orinnano.yml down

# Restart
docker-compose -f docker/docker-compose.orinnano.yml restart

# Check status
docker ps | grep vlm
curl http://localhost:8000/health  # Should return {"status":"ok"}
```

## Performance Tuning

In `docker-compose.orinnano.yml`, adjust:

```yaml
environment:
  - VLM_MODEL=Qwen/Qwen2.5-VL-3B-Instruct  # Change model here
  - USE_QUANTIZATION=true  # Enable/disable quantization

deploy:
  resources:
    limits:
      memory: 6G    # Adjust based on available VRAM
    reservations:
      memory: 3G    # Minimum guaranteed memory
```

## Model Alternatives

Tested on Jetson Orin Nano:

| Model | Size | VRAM | Speed | Notes |
|-------|------|------|-------|-------|
| Qwen2.5-VL-3B-Instruct | 3B | 5-6GB | ~2-3s | Default, good balance |
| SmolVLM-256M | 256M | 1-2GB | ~0.5s | Ultra-lightweight |
| LLaVA-1.5-7B | 7B | 12GB+ | ~5s | Requires Orin with 16GB VRAM |

To switch models:

```bash
# Edit docker/Dockerfile.orinnano or docker-compose:
export VLM_MODEL="SmolVLM-256M"
docker-compose -f docker/docker-compose.orinnano.yml build --no-cache
docker-compose -f docker/docker-compose.orinnano.yml up -d
```

## Stopping the Service

The node automatically stops the container when ROS2 is shut down:

```bash
# Kill ROS2 launch (Ctrl+C)
# Container is automatically stopped

# Or manually
docker-compose -f docker/docker-compose.orinnano.yml down
```

## Advanced: Custom Image Processing

Edit `inference_server.py` to preprocess images (resize, normalize, etc.):

```python
# In run_inference():
image = image.resize((896, 672))  # Qwen2.5-VL expects this
```

Rebuild: `docker-compose build --no-cache`

---

**Next Steps:**
1. ✓ Follow setup steps above
2. ✓ Run test script
3. ✓ Launch ROS2 nodes
4. ✓ Test with real camera feed
5. ✓ Tune prompts for your robot's task
