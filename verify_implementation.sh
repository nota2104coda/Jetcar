#!/bin/bash
# Verify SmolVLA implementation files
set -e

echo "🔍 Verifying SmolVLA Implementation..."
echo "═════════════════════════════════════════"

REPO="/home/jeevan/PicoWCar"
cd "$REPO"

files=(
    "docker/Dockerfile.orinnano"
    "docker/docker-compose.orinnano.yml"
    "docker/vlm_container/inference_server.py"
    "docker/vlm_container/scripts/test_vlm_service.py"
    "src/pi5_pkg/pi5_pkg/smolvla_node.py"
    "src/pi5_pkg/launch/vlm_navigation.launch.py"
    "docs/JETSON_VLM_SETUP.md"
    "SMOLVLA_DESIGN_RATIONALE.md"
    "SMOLVLA_IMPLEMENTATION.md"
    "SMOLVLA_QUICKSTART.txt"
    "IMPLEMENTATION_SUMMARY.md"
    "scripts/start-smolvla.sh"
)

echo ""
all_present=true
for file in "${files[@]}"; do
    if [ -f "$file" ]; then
        size=$(wc -l < "$file")
        echo "✓ $file ($size lines)"
    else
        echo "✗ MISSING: $file"
        all_present=false
    fi
done

echo ""
echo "═════════════════════════════════════════"

if [ "$all_present" = true ]; then
    echo "✅ All implementation files present!"
    echo ""
    echo "Next steps:"
    echo "1. Read quick reference:"
    echo "   cat SMOLVLA_QUICKSTART.txt"
    echo ""
    echo "2. Run automated setup:"
    echo "   bash scripts/start-smolvla.sh"
    echo ""
    echo "3. Launch ROS2 nodes:"
    echo "   source /opt/ros/humble/setup.bash"
    echo "   source install/setup.bash"
    echo "   ros2 launch pi5_pkg vlm_navigation.launch.py"
    echo ""
    exit 0
else
    echo "❌ Some files are missing. Check above."
    exit 1
fi
