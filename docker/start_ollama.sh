#!/bin/bash
# Start Ollama Moondream container for PicoWCar VLM inference
# For Jetson Orin Nano with JetPack 6.2 (L4T 36.4.0)

set -e

# Configuration
CONTAINER_NAME="picowcar-ollama-service"
DOCKER_IMAGE="dustynv/ollama:main-r36.4.0"
OLLAMA_MODEL="${OLLAMA_MODEL:-moondream}"
CACHE_DIR="${VLM_MODELS_DIR:-/mnt/nvme/cache}"
OLLAMA_CACHE="${CACHE_DIR}/ollama"
HF_CACHE="${CACHE_DIR}"

echo "=========================================="
echo "PicoWCar Ollama Moondream Container"
echo "=========================================="
echo "Container: ${CONTAINER_NAME}"
echo "Image: ${DOCKER_IMAGE}"
echo "Model: ${OLLAMA_MODEL}"
echo "Cache: ${OLLAMA_CACHE}"
echo "=========================================="

# Check if container is already running
if docker ps --filter "name=${CONTAINER_NAME}" --format "{{.Names}}" | grep -q "${CONTAINER_NAME}"; then
    echo "✓ Container ${CONTAINER_NAME} is already running"
    exit 0
fi

# Stop and remove any existing stopped container
if docker ps -a --filter "name=${CONTAINER_NAME}" --format "{{.Names}}" | grep -q "${CONTAINER_NAME}"; then
    echo "Removing existing stopped container..."
    docker rm -f "${CONTAINER_NAME}" 2>/dev/null || true
fi

# Create cache directories
echo "Creating cache directories..."
mkdir -p "${OLLAMA_CACHE}"
mkdir -p "${HF_CACHE}"

# Pull latest image (optional)
if [ "${DOCKER_PULL:-}" = "always" ]; then
    echo "Pulling latest image..."
    docker pull "${DOCKER_IMAGE}"
fi

# Start container
echo "Starting Ollama container..."
docker run -d \
    --name "${CONTAINER_NAME}" \
    --rm \
    --gpus all \
    -p 9000:9000 \
    -e "OLLAMA_MODEL=${OLLAMA_MODEL}" \
    -e "OLLAMA_MODELS=/root/.ollama" \
    -e "OLLAMA_HOST=0.0.0.0:9000" \
    -e "OLLAMA_CONTEXT_LEN=4096" \
    -e "OLLAMA_LOGS=/root/.ollama/ollama.log" \
    -e "HF_HUB_CACHE=/root/.cache/huggingface" \
    ${HF_TOKEN:+-e "HF_TOKEN=${HF_TOKEN}"} \
    -v "${OLLAMA_CACHE}:/root/.ollama" \
    -v "${HF_CACHE}:/root/.cache" \
    "${DOCKER_IMAGE}"

# Wait for service to be ready
echo "Waiting for Ollama service to be ready..."
MAX_RETRIES=60
for i in $(seq 1 ${MAX_RETRIES}); do
    if curl -s http://localhost:9000/api/version > /dev/null 2>&1; then
        echo "✓ Ollama service is ready!"
        break
    fi
    
    if [ $i -eq ${MAX_RETRIES} ]; then
        echo "ERROR: Ollama service failed to start within timeout"
        docker logs "${CONTAINER_NAME}" --tail 50
        exit 1
    fi
    
    if [ $((i % 10)) -eq 0 ]; then
        echo "  Waiting... (${i}/${MAX_RETRIES})"
    fi
    
    sleep 2
done

echo ""
echo "=========================================="
echo "✓ Ollama Moondream is ready!"
echo "=========================================="
echo "API endpoint: http://localhost:9000/v1/chat/completions"
echo "Health check: http://localhost:9000/api/version"
echo ""
echo "Test with:"
echo "  cd docker && python3 test_ollama_moondream.py test_sparrow.jpg"
echo ""
echo "Stop with:"
echo "  docker stop ${CONTAINER_NAME}"
echo "=========================================="
