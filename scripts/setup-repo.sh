#!/bin/bash
#setup repo to work with Jetcar project. This script assumes a fresh Ubuntu 22.04 or 24.04 install on a Jetson Nano, and will set up the necessary environment to work with the Jetcar repository.

# --- Configuration Variables ---
USERNAME="xyz" # CHANGE THIS to your computer's username
EMAIL="xyz@gmail.com" # CHANGE THIS
GIT_USER="xyz" # CHANGE THIS
REPO_URL="git@github.com:nota2104coda/Jetcar.git"
REPO_DIR="/home/$USERNAME/Jetcar"
git clone --recurse-submodules $REPO_URL $REPO_DIR
cd $REPO_DIR
git submodule update --init --recursive

# --- 7. Virtual Environment and ROS 2 Python Setup ---
echo "--- 7. Setting up Python Virtual Environment and installing requirements ---"

# Create and activate venv
python3 -m venv ~$REPO_DIR/.venv
source ~$REPO_DIR/.venv/bin/activate

pip install -r ~$REPO_DIR/jetcar-requirements.txt

# Add ROS 2 setup to venv activation (assuming core ROS 2 is installed elsewhere)
# If you didn't install core ROS 2 via apt, you must install it first!
echo "source /opt/ros/$ROS2_DISTRO/setup.bash" >> ~$REPO_DIR/.venv/bin/activate
source ~$REPO_DIR/.venv/bin/activate


