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

# 2. Shortcut to launch from jetsoncpp_pkg (jl)
# Usage: jl gazebo.launch.py
jl() {
   if [ -z "$1" ]; then
       echo "Usage: jl <launch_file>"
       return 1
   # Check if the specific file exists in the package launch directory
   elif [ ! -f "$HOME/Jetcar/src/jetsoncpp_pkg/launch/$1" ]; then
        echo "Warning: '$1' not found in jetsoncpp_pkg/launch/"
        echo "Running anyway in case it's a system file..."
        ros2 launch jetsoncpp_pkg "$@"
   else
        ros2 launch jetsoncpp_pkg "$@"
   fi
}
 
# 3. Shortcut to build the workspace
cb() {
   colcon build --symlink-install "$@"
}

# 4. Shortcut to source the system ROS 2 environment
alias sros='source /opt/ros/humble/setup.bash'
EOF
    echo "Shortcuts added to ~/.bashrc"
else
    echo "Shortcuts already exist in ~/.bashrc"
fi
