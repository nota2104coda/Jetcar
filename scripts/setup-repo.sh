#!/bin/bash
#setup repo to work with Jetcar project. This script assumes a fresh Ubuntu 22.04 or 24.04 install on a Jetson Nano, and will set up the necessary environment to work with the Jetcar repository.

# --- Configuration Variables ---
USERNAME=$(whoami) # CHANGE THIS to your computer's username if needed
EMAIL="xyz@gmail.com" # CHANGE THIS
GIT_USER="xyz" # CHANGE THIS
REPO_URL="git@github.com:nota2104coda/Jetcar.git"
REPO_DIR="$HOME/Jetcar"
UBUNTU_VER=$(lsb_release -rs)
if [ "$UBUNTU_VER" = "22.04" ]; then
    ROS2_DISTRO="humble"
elif [ "$UBUNTU_VER" = "24.04" ]; then
    ROS2_DISTRO="jazzy"
else
    echo "Ubuntu version not supported, exiting..."
    exit 1
fi

if [ ! -d "$REPO_DIR" ]; then
    git clone --recurse-submodules $REPO_URL $REPO_DIR
fi
cd $REPO_DIR
git submodule update --init --recursive

# --- 7. Virtual Environment and ROS 2 Python Setup ---
echo "--- 7. Setting up Python Virtual Environment and installing requirements ---"

# Create and activate venv
python3 -m venv $REPO_DIR/.venv
source $REPO_DIR/.venv/bin/activate

# Jetson-specific optimizations for aarch64 (JetPack 7.2 / CUDA 13.2)
if [ "$(uname -m)" = "aarch64" ]; then
    echo "--- Detected Jetson (aarch64). Applying optimizations for JetPack 7.2... ---"
    sudo apt update
    sudo apt install libopencv-dev python3-opencv -y
    
    # Check and append PIP_EXTRA_INDEX_URL if not present
    if ! grep -q "PIP_EXTRA_INDEX_URL" "$REPO_DIR/.venv/bin/activate"; then
        echo "export PIP_EXTRA_INDEX_URL=https://pypi.jetson-ai-lab.io/jp7/cu132" >> $REPO_DIR/.venv/bin/activate
    fi
    
    # Check and append LD_LIBRARY_PATH if not present
    if ! grep -q "LD_LIBRARY_PATH" "$REPO_DIR/.venv/bin/activate"; then
        echo 'export LD_LIBRARY_PATH="/usr/lib/aarch64-linux-gnu/tegra:$LD_LIBRARY_PATH"' >> $REPO_DIR/.venv/bin/activate
    fi
    
    # Check and append ROS 2 setup if not present
    if ! grep -q "source /opt/ros/$ROS2_DISTRO/setup.bash" "$REPO_DIR/.venv/bin/activate"; then
        echo "source /opt/ros/$ROS2_DISTRO/setup.bash" >> $REPO_DIR/.venv/bin/activate
    fi

    # Install optimized OpenCV
    pip install opencv-python-headless
fi

pip install -r $REPO_DIR/jetcar-requirements.txt

# Add ROS 2 setup to venv activation (assuming core ROS 2 is installed elsewhere)
# If you didn't install core ROS 2 via apt, you must install it first!
# Append ROS 2 setup to venv activation (with grep guards) so it's ready for future sessions
if ! grep -q "setup.bash" "$REPO_DIR/.venv/bin/activate"; then
    echo "source /opt/ros/$ROS2_DISTRO/setup.bash" >> $REPO_DIR/.venv/bin/activate
fi

echo "Virtual environment configuration complete."


