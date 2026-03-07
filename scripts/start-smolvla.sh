#!/bin/bash
# Quick start script for SmolVLA with Jetson Orin Nano
# Usage: ./scripts/start-smolvla.sh

set -e

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

echo "=================================================="
echo "SmolVLA Quick Start"
echo "=================================================="

# Check prerequisites
echo "[1/5] Checking prerequisites..."

if ! command -v docker &> /dev/null; then
    echo "✗ Docker not found. Install with: apt install docker.io"
    exit 1
fi

if ! docker ps &> /dev/null; then
    echo "✗ Docker not accessible. Add user to docker group:"
    echo "   sudo usermod -aG docker \$USER && newgrp docker"
    exit 1
fi

if [ ! -d /mnt/nvme/cache ]; then
    echo "✗ Model cache directory not found. Creating..."
    sudo mkdir -p /mnt/nvme/cache
    sudo chmod 755 /mnt/nvme/cache
fi

echo "✓ Prerequisites OK"

# Build image
echo ""
echo "[2/5] Building VLM container image..."

if docker images | grep -q "picowcar/jetson-vlm"; then
    echo "  Image exists. Skipping build."
    echo "  (To rebuild: docker-compose -f docker/docker-compose.orinnano.yml build --no-cache)"
else
    docker-compose -f docker/docker-compose.orinnano.yml build
fi

echo "✓ Image ready"

# Start service
echo ""
echo "[3/5] Starting VLM inference service..."

docker-compose -f docker/docker-compose.orinnano.yml up -d

echo "✓ Service started"

# Wait for health check
echo ""
echo "[4/5] Waiting for service to be ready..."

max_retries=60
retry=0
while [ $retry -lt $max_retries ]; do
    if curl -sf http://localhost:8000/health > /dev/null 2>&1; then
        echo "✓ Service is healthy!"
        break
    fi
    retry=$((retry + 1))
    if [ $((retry % 10)) -eq 0 ]; then
        echo "  Still waiting... ($retry/$max_retries)"
    fi
    sleep 1
done

if [ $retry -eq $max_retries ]; then
    echo "✗ Service failed to become healthy"
    docker logs --tail 20 picowcar-vlm-service
    exit 1
fi

# Test service
echo ""
echo "[5/5] Testing inference (this may take a moment on first run)..."

python3 docker/vlm_container/scripts/test_vlm_service.py

echo ""
echo "=================================================="
echo "✓ SmolVLA is ready!"
echo "=================================================="
echo ""
echo "Next steps:"
echo "  1. Source ROS2:"
echo "     source /opt/ros/humble/setup.bash"
echo "     source install/setup.bash"
echo ""
echo "  2. Launch ROS2 nodes:"
echo "     ros2 launch python_pkg vlm_navigation.launch.py"
echo ""
echo "  3. Monitor output:"
echo "     ros2 topic echo /cmd_vel"
echo ""
