#!/bin/bash
# Quick Start Guide for Ollama Moondream Integration
# Run this script to set up and test the new VLM system

echo "╔════════════════════════════════════════════════════════════╗"
echo "║  PicoWCar Ollama Moondream Integration - Quick Start      ║"
echo "╚════════════════════════════════════════════════════════════╝"
echo ""

# Check if we're on Jetson
if [ ! -f /etc/nv_tegra_release ]; then
    echo "⚠️  Warning: This script is designed for Jetson Orin Nano"
    echo "   Current system: $(uname -n)"
    echo ""
    read -p "Continue anyway? (y/n) " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        exit 0
    fi
fi

echo "Step 1: Check Prerequisites"
echo "─────────────────────────────"

# Check Docker
if ! command -v docker &> /dev/null; then
    echo "❌ Docker not found. Please install Docker first."
    exit 1
fi
echo "✅ Docker installed"

# Check NVIDIA runtime
if ! docker run --rm --gpus all nvidia/cuda:11.4.0-base-ubuntu20.04 nvidia-smi &> /dev/null; then
    echo "❌ NVIDIA Docker runtime not available"
    exit 1
fi
echo "✅ NVIDIA runtime available"

# Check cache directory
CACHE_DIR="${VLM_MODELS_DIR:-/mnt/nvme/cache}"
if [ ! -d "$CACHE_DIR" ]; then
    echo "⚠️  Cache directory $CACHE_DIR not found"
    read -p "Create it? (y/n) " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        sudo mkdir -p "$CACHE_DIR"
        sudo chown $USER:$USER "$CACHE_DIR"
        echo "✅ Created $CACHE_DIR"
    fi
fi

echo ""
echo "Step 2: Start Ollama Container"
echo "─────────────────────────────"
cd docker
./start_ollama.sh

if [ $? -ne 0 ]; then
    echo "❌ Failed to start Ollama container"
    exit 1
fi

echo ""
echo "Step 3: Test Ollama API"
echo "─────────────────────────────"

if [ -f "test_sparrow.jpg" ]; then
    echo "Testing with test_sparrow.jpg..."
    python3 test_ollama_moondream.py test_sparrow.jpg
    
    if [ $? -eq 0 ]; then
        echo "✅ API test passed!"
    else
        echo "❌ API test failed"
        exit 1
    fi
else
    echo "⚠️  test_sparrow.jpg not found, skipping API test"
fi

echo ""
echo "Step 4: Build ROS2 Package (Optional)"
echo "─────────────────────────────"
read -p "Build python_pkg with updated smolvla_node? (y/n) " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    cd ..
    colcon build --packages-select python_pkg
    source install/setup.bash
    echo "✅ Package built successfully"
fi

echo ""
echo "╔════════════════════════════════════════════════════════════╗"
echo "║  Setup Complete! 🎉                                        ║"
echo "╚════════════════════════════════════════════════════════════╝"
echo ""
echo "Next Steps:"
echo "───────────"
echo ""
echo "1. Test ROS2 node (if built):"
echo "   ros2 run python_pkg smolvla_node"
echo ""
echo "2. Monitor Ollama container:"
echo "   docker logs -f picowcar-ollama-service"
echo ""
echo "3. Check GPU usage:"
echo "   nvidia-smi"
echo "   tegrastats"
echo ""
echo "4. Stop container:"
echo "   docker stop picowcar-ollama-service"
echo ""
echo "5. Read documentation:"
echo "   - docker/OLLAMA_INTEGRATION.md"
echo "   - OLLAMA_MIGRATION.md"
echo ""
echo "Container Info:"
echo "  Name: picowcar-ollama-service"
echo "  Port: 9000"
echo "  API: http://localhost:9000/v1/chat/completions"
echo "  Model: moondream"
echo "  Cache: $CACHE_DIR/ollama"
echo ""
