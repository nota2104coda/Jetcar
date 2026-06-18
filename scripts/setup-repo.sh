#!/bin/bash
#setup repo to work with Jetcar project. This script assumes a fresh Ubuntu 22.04 or 24.04 install on a Jetson Nano, and will set up the necessary environment to work with the Jetcar repository.

# --- Configuration Variables ---
USERNAME="xyz" # CHANGE THIS to your computer's username
EMAIL="xyz@gmail.com" # CHANGE THIS
GIT_USER="xyz" # CHANGE THIS
ROS2_DISTRO="humble" # Assuming you're on a 22.04 base. If 24.04, change to "jazzy"
REPO_URL="git@github.com:nota2104coda/Jetcar.git"
REPO_DIR="/home/$USERNAME/Jetcar"
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
    
    # Set custom repository index for optimized wheels
    export PIP_EXTRA_INDEX_URL=https://pypi.jetson-ai-lab.io/jp7/cu132
    echo "export PIP_EXTRA_INDEX_URL=https://pypi.jetson-ai-lab.io/jp7/cu132" >> $REPO_DIR/.venv/bin/activate
    
    # Add Tegra libraries to path
    echo 'export LD_LIBRARY_PATH="/usr/lib/aarch64-linux-gnu/tegra:$LD_LIBRARY_PATH"' >> $REPO_DIR/.venv/bin/activate
    export LD_LIBRARY_PATH="/usr/lib/aarch64-linux-gnu/tegra:$LD_LIBRARY_PATH"
    
    # Install optimized OpenCV
    pip install opencv-python-headless
fi

pip install -r $REPO_DIR/jetcar-requirements.txt

# Add ROS 2 setup to venv activation (assuming core ROS 2 is installed elsewhere)
# If you didn't install core ROS 2 via apt, you must install it first!
echo "source /opt/ros/$ROS2_DISTRO/setup.bash" >> $REPO_DIR/.venv/bin/activate
source $REPO_DIR/.venv/bin/activate

# --- 8. Add shortcuts to .bashrc ---
echo "--- 8. Adding shortcuts to ~/.bashrc ---"

BASHRC="$HOME/.bashrc"

# Check if the aliases already exist to avoid duplication
if ! grep -q "alias sdev=" "$BASHRC"; then
cat << 'EOF' >> "$BASHRC"

# --- Jetcar Workspace Shortcuts ---
# 1. Shortcut to source the workspace (sdev)
alias sdev='source ~/Jetcar/install/setup.bash && echo "sourced install/setup.bash" '

# 2. Shortcut to launch from jetcar_bringup (jl)
# Usage: jl gazebo.launch.py
jl() {
   if [ -z "$1" ]; then
       echo "Usage: jl <launch_file>"
       return 1
   # Check if the specific file exists in the package launch directory
   elif [ ! -f "$HOME/Jetcar/src/jetcar_bringup/launch/$1" ]; then
        echo "Warning: '$1' not found in jetcar_bringup/launch/"
        echo "Running anyway in case it's a system file..."
        ros2 launch jetcar_bringup "$@"
   else
        ros2 launch jetcar_bringup "$@"
   fi
}
 
# 3. Shortcut to build the workspace
cb() {
   curr_dir=$(pwd)
   cd $HOME/Jetcar
   if colcon build --symlink-install "$@"; then
	  source install/setup.bash
        echo "Build success and sourced"
   else
	echo "Build failed"
   fi
   # go back to previous dir
   cd "$curr_dir"

}

# 4. Shortcut to source the system ROS 2 environment
alias sros='source /opt/ros/humble/setup.bash'
EOF
    echo "Shortcuts added to ~/.bashrc"
else
    echo "Shortcuts already exist in ~/.bashrc"
fi
