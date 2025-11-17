#!/bin/bash
# Complete end-to-end SmolVLA setup and launch guide
# Copy-paste these commands sequentially

echo "=========================================="
echo "SmolVLA End-to-End Setup"
echo "=========================================="

# Step 1: Setup prerequisites
echo ""
echo "Step 1: Setup prerequisites..."
echo "  → Creating model cache directory"
sudo mkdir -p /mnt/nvme/cache
sudo chmod 755 /mnt/nvme/cache
echo "  → Adding user to docker group"
sudo usermod -aG docker $USER
echo "  → New group session (or logout/login): newgrp docker"
echo ""
echo "Run this to activate docker group for current session:"
echo "  newgrp docker"
echo ""

# Step 2: Build image
echo ""
echo "Step 2: Build Docker image..."
echo "  cd /home/jeevan/PicoWCar"
echo "  docker-compose -f docker/docker-compose.orinnano.yml build"
echo ""

# Step 3: Start service
echo ""
echo "Step 3: Start VLM inference service..."
echo "  docker-compose -f docker/docker-compose.orinnano.yml up -d"
echo ""
echo "Monitor startup:"
echo "  docker logs -f picowcar-vlm-service"
echo ""
echo "Wait for message: '✓ Model loaded and ready!'"
echo ""

# Step 4: Test
echo ""
echo "Step 4: Test inference..."
echo "  python3 docker/vlm_container/scripts/test_vlm_service.py"
echo ""

# Step 5: Build ROS2 package
echo ""
echo "Step 5: Build ROS2 package..."
echo "  cd /home/jeevan/PicoWCar"
echo "  source /opt/ros/humble/setup.bash"
echo "  colcon build --packages-select pi5_pkg"
echo ""

# Step 6: Launch
echo ""
echo "Step 6: Launch ROS2 nodes..."
echo "  Terminal 1:"
echo "    cd /home/jeevan/PicoWCar"
echo "    source /opt/ros/humble/setup.bash"
echo "    source install/setup.bash"
echo "    ros2 launch pi5_pkg vlm_navigation.launch.py"
echo ""
echo "  Terminal 2 (optional - monitor commands):"
echo "    ros2 topic echo /cmd_vel"
echo ""

# Step 7: Stop
echo ""
echo "Step 7: Shutdown..."
echo "  Press Ctrl+C in launch terminal"
echo "  Container stops automatically"
echo ""

# Step 8: Advanced - manual container control
echo ""
echo "Advanced Commands:"
echo "  # Manually control service"
echo "  docker-compose -f docker/docker-compose.orinnano.yml restart"
echo "  docker-compose -f docker/docker-compose.orinnano.yml down"
echo ""
echo "  # Check container status"
echo "  docker ps | grep vlm"
echo "  curl http://localhost:8000/health"
echo ""
echo "  # View inference logs"
echo "  docker logs -f picowcar-vlm-service"
echo ""
echo "  # Custom prompt"
echo "  ros2 launch pi5_pkg vlm_navigation.launch.py vlm_prompt:='Find and follow the ball'"
echo ""

echo "=========================================="
echo "Setup complete!"
echo "=========================================="
