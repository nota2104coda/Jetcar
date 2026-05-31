#!/bin/bash
set -e
#this script could be used to run the ROS2 system on boot, by adding it to /etc/rc.local or using systemd
# Download models on first run if not cached
if [ ! -d "$HF_HOME" ] || [ -z "$(ls -A $HF_HOME)" ]; then
    echo "Downloading VLM model for first time..."
    /usr/local/bin/download_model.sh
fi

# Source ROS2
source /opt/ros/iron/setup.bash
source /opt/ros_ws/install/setup.bash

# Start ROS2 launch
echo "Starting ROS2 nodes..."
ros2 launch jetson_vlm_pkg vlm_launch.py

exec "$@"
