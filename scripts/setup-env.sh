#!/bin/bash
# setup ROS2 environment, NoMachine, and other developer necessities.

# --- Configuration Variables ---
USERNAME=$(whoami) # CHANGE THIS to your computer's username if needed
EMAIL="xyz@gmail.com" # CHANGE THIS
GIT_USER="xyz" # CHANGE THIS
RDP_PASSWORD="secure_password" # CHANGE THIS for headless remote desktop password
UBUNTU_VER=$(lsb_release -rs)
if [ "$UBUNTU_VER" = "22.04" ]; then
    ROS2_DISTRO="humble"
elif [ "$UBUNTU_VER" = "24.04" ]; then
    ROS2_DISTRO="jazzy"
else
    echo "Ubuntu version not supported, exiting..."
    exit 1
fi
export ARCH=$(uname -m)
JETSON_IP="192.168.50.177" # CHANGE THIS to your Jetson's IP address on the network
echo "computer architecture detected: $ARCH"
echo "Ubuntu version detected: $(lsb_release -rs)"
echo "ROS2 distribution selected: $ROS2_DISTRO"

# --- 1. System Update and Dependencies ---
echo "--- 1. Updating System and Installing Core Dependencies ---"
sudo apt update
sudo apt upgrade -y
sudo apt install git libsecret-1-0 libsecret-1-dev python3-venv -y

# --- 2. SSH and Firewall Setup ---
echo "--- 2. SSH and Firewall Setup ---"
# Note: For fresh flashes, ssh is often enabled by default, but we ensure it and UFW.
sudo apt install openssh-server -y
sudo systemctl enable ssh
sudo systemctl start ssh
sudo ufw allow ssh
sudo ufw --force enable

#To enable remote desktop
if [ "$ARCH" = "aarch64" ]; then
    echo "--- Setting up headless RDP for Jetson ARM64 ---"
    
    # 1. Ensure dependencies are present
    sudo apt update && sudo apt install winpr-utils -y

    # 2. Create the System Certificate Directory first if not present
    TARGET_DIR="/var/lib/gnome-remote-desktop/.local/share/gnome-remote-desktop"
    if [ ! -d "$TARGET_DIR" ]; then
        sudo mkdir -p "$TARGET_DIR"
    fi

    # 3. Generate FreeRDP native certificates directly as the system user
    # This prevents permission mismatch errors down the pipeline
    sudo -u gnome-remote-desktop winpr-makecert -silent -rdp -path "$TARGET_DIR" tls

    # 4. Enforce strict system ownership permissions 
    sudo chown -R gnome-remote-desktop:gnome-remote-desktop /var/lib/gnome-remote-desktop/

    # 5. Bind the native keys to the system RDP profile
    sudo grdctl --system rdp set-tls-key "$TARGET_DIR/tls.key"
    sudo grdctl --system rdp set-tls-cert "$TARGET_DIR/tls.crt"

    # 6. Configure credentials and display options system-wide
    sudo grdctl --system rdp set-credentials $USERNAME "$RDP_PASSWORD"
    sudo grdctl --system rdp disable-view-only

    # 7. Enable the backend service wrapper
    sudo grdctl --system rdp enable

    # 8. Restart display management layers to apply new configurations
    sudo systemctl restart gnome-remote-desktop.service
    sudo systemctl restart gdm3
    
    echo "--- Headless RDP Configuration Complete. Run 'sudo grdctl --system status' to verify. ---"
fi

# --- 4. Git and SSH Key Generation ---
echo "--- 4. Setting up Git and generating SSH Key ---"
git config --global user.name "$GIT_USER"
git config --global user.email "$EMAIL"

# Check for existing key and generate if not found
if [ ! -f ~/.ssh/id_ed25519 ]; then
    ssh-keygen -t ed25519 -C "$EMAIL" -N "" # -N "" means no passphrase
fi

echo -e "\n\n!!! ACTION REQUIRED: Copy your public key to GitHub !!!"
echo "Public Key (Copy this entire block):"
cat ~/.ssh/id_ed25519.pub
echo "!!! After adding to GitHub, run 'ssh -T git@github.com' to confirm !!!\n"
echo "waiting 120sec for you to add the key to GitHub...
#Copy the public key: Use the cat command to display the contents of your public key file and copy it to your clipboard.

#Highlight and copy the entire output, starting with ssh-ed25519 and ending with your email.
#Add the key to your GitHub account:
#Log in to your GitHub account.
#Go to Settings (found by clicking your profile picture in the top-right corner).
#In the left sidebar, click SSH and GPG keys.
#Click the New SSH key button.
#Give your key a descriptive Title (e.g., "Raspberry Pi 5").
#Paste the entire public key you copied in the Key field.
#Click Add SSH key. Enter your GitHub password to confirm. "

sleep 120

ssh -T git@github.com

# --- 1. ROS 2 Installation Procedure ---
echo "--- 1. Setting up ROS 2 Humble/Jazzy Repository and Installing ros-base ---"

# Set Locale (Critical for ROS 2)
sudo apt install locales -y
sudo locale-gen en_US en_US.UTF-8
sudo update-locale LC_ALL=en_US.UTF-8 LANG=en_US.UTF-8
export LANG=en_US.UTF-8

# Add ROS 2 GPG key and Repository Source List
sudo apt install curl gnupg lsb-release software-properties-common -y
sudo add-apt-repository universe
sudo curl -sSL https://raw.githubusercontent.com/ros/rosdistro/master/ros.key -o /usr/share/keyrings/ros-archive-keyring.gpg
echo "deb [arch=$(dpkg --print-architecture) signed-by=/usr/share/keyrings/ros-archive-keyring.gpg] http://packages.ros.org/ros2/ubuntu $(. /etc/os-release && echo $UBUNTU_CODENAME) main" | sudo tee /etc/apt/sources.list.d/ros2.list > /dev/null
# For Zenoh middleware dependencies
echo "deb [trusted=yes] https://pkg.zenoh.io/debian all main" | sudo tee /etc/apt/sources.list.d/zenoh.list

# Update package cache
sudo apt update

# Install the ROS 2 Humble base package (Minimal core tools and communication)
sudo apt install ros-$ROS2_DISTRO-ros-base -y

#install rosbridge_server to cnnect foxglove visualisation
sudo apt install ros-$ROS2_DISTRO-foxglove-bridge -y

# Install development tools (colcon, etc.)
sudo apt install python3-colcon-common-extensions ros-dev-tools -y

echo "--- Installing Common ROS 2 Packages ---"
sudo apt install -y \
   ros-$ROS2_DISTRO-robot-localization \
   ros-$ROS2_DISTRO-slam-toolbox \
   ros-$ROS2_DISTRO-image-transport \
   ros-$ROS2_DISTRO-image-transport-plugins \
   ros-$ROS2_DISTRO-compressed-image-transport \
   ros-$ROS2_DISTRO-tf2-tools \
   ros-$ROS2_DISTRO-nav2-bringup \
   ros-$ROS2_DISTRO-nav2-controller \
   ros-$ROS2_DISTRO-nav2-planner \
   ros-$ROS2_DISTRO-nav2-smoother \
   ros-$ROS2_DISTRO-nav2-behaviors \
   ros-$ROS2_DISTRO-nav2-bt-navigator \
   ros-$ROS2_DISTRO-nav2-waypoint-follower \
   ros-$ROS2_DISTRO-nav2-velocity-smoother \
   ros-$ROS2_DISTRO-ros2-control \
   ros-$ROS2_DISTRO-ros2-controllers \
   ros-$ROS2_DISTRO-realsense2-camera \
   ros-$ROS2_DISTRO-realsense2-description \
   ros-$ROS2_DISTRO-realsense2-camera-msgs \
   ros-$ROS2_DISTRO-rqt \
   ros-$ROS2_DISTRO-rqt-common-plugins \
   ros-$ROS2_DISTRO-topic-tools \
   ros-$ROS2_DISTRO-rmw-zenoh-cpp 
   

if [ "$ARCH" = "x86_64" ]; then
    echo "--- Installing PC/Simulation Packages (x86_64) ---"
    if [ "$ROS2_DISTRO" = "jazzy" ]; then
        sudo apt install -y \
           ros-$ROS2_DISTRO-gz-ros2-control \
           ros-$ROS2_DISTRO-ros-gz
    else
        sudo apt install -y \
           ros-$ROS2_DISTRO-gazebo-ros2-control \
           ros-$ROS2_DISTRO-ros-gz \
           ros-$ROS2_DISTRO-ros-gz-sim \
           ros-$ROS2_DISTRO-ros-gz-sim-sensors \
           ros-$ROS2_DISTRO-ros-gz-sim-plugins
    fi
elif [ "$ARCH" = "aarch64" ]; then
    echo "--- Installing Jetson/Robot Packages (aarch64) ---"
    sudo apt install -y \
       ros-$ROS2_DISTRO-isaac-ros-nvblox \
       ros-$ROS2_DISTRO-isaac-ros-visual-slam \
       ros-$ROS2_DISTRO-nvblox-ros \
       ros-$ROS2_DISTRO-nvblox-nav2
    
    # Verify Isaac ROS package installation paths
    ros2 pkg prefix isaac_ros_nvblox && ros2 pkg prefix isaac_ros_visual_slam
fi

# Add user to dialout group for serial/Lidar access
echo "--- Adding user to dialout group ---"
sudo usermod -aG dialout $USER

# --- 8. Environment Configuration (.bashrc) ---
echo "--- 8. Configuring .bashrc and User Environment ---"

BASHRC="$HOME/.bashrc"

if ! grep -q "# Jetcar Environment Configuration" "$BASHRC"; then
cat << EOF >> "$BASHRC"

# Jetcar Environment Configuration
# some more of my aliases
# 1. Shortcut to source the workspace (sdev)
REPO_DIR="\$HOME/Jetcar"
ROS2_DISTRO="$ROS2_DISTRO" # Assuming you're on a 22.04 base. If 24.04, change to "jazzy"

alias vv='source $REPO_DIR/.venv/bin/activate' # activate virtual environment 
alias sdev='source ~/Jetcar/install/setup.bash && echo "sourced install/setup.bash" '
alias sros='source /opt/ros/$ROS2_DISTRO/setup.bash'
alias rlgaz='ros2 launch jetcar_sim gazebo.launch.py'

# 2. Shortcut to launch from jetcar_bringup (jl)
# Usage: jl gazebo.launch.py
jl() {
   if [ -z "\$1" ]; then
     echo "Usage: jl <launch_file>"
     return 1
   elif [ ! -f "~/Jetcar/src/jetcar_bringup/launch/\$1" ]; then
     echo "Warning: '\$1' not found in jetcar_bringup/launch/"
     echo "Running anyway in case it's a system file..."
     ros2 launch jetcar_bringup "\$@"
   else
     ros2 launch jetcar_bringup "\$@"
   fi
}

# 3. Recommended: Shortcut to build the workspace
#alias cb='colcon build --symlink-install'

cb() {
   curr_dir=$(pwd)
   cd $HOME/Jetcar
   if colcon build --symlink-install "\$@" --cmake-args -DCMAKE_CXX_FLAGS="-include pthread.h"; then
     source install/setup.bash
     echo "Build success and sourced"
   else
     echo "Build failed"
   fi
   # go back to previous dir
   cd "$curr_dir"
}

alias killros='
  echo "Stopping all ROS 2 nodes...";
  # 1. Send SIGINT (Ctrl+C) to all ROS/Gazebo processes first so they close cleanly
  pkill -INT -f "ros2 launch";
  pkill -INT -f "_node";
  pkill -INT -f "gzserver";
  
  # 2. Wait 3 seconds for them to release ports and hardware
  sleep 3;
  
  # 3. Force-kill any stubborn remaining processes
  pkill -9 -f "ros2 launch";
  pkill -9 -f "_node";
  pkill -9 -f "gzserver";
  pkill -9 -f "gzclient";
  echo "All ROS 2 and Gazebo processes terminated."
'

#start or ensure zenoh via systemd
export RMW_IMPLEMENTATION=rmw_zenoh_cpp
if ! systemctl --user is-active --quiet zenoh-router.service; then
    echo "Zenoh router service is not running. Starting it via systemd..."
    systemctl --user start zenoh-router.service
fi

source /opt/ros/$ROS2_DISTRO/setup.bash

EOF
    echo "Shortcuts and ROS configuration added to ~/.bashrc"
else
    echo "Shortcuts already exist in ~/.bashrc"
fi

#To run Gemini or Copilot CLI
#---8. Install copilot CLI and gemini CLI
sudo apt install curl
curl -o- https://raw.githubusercontent.com/nvm-sh/nvm/v0.40.1/install.sh | bash
# Load nvm directly for this script execution
export NVM_DIR="$HOME/.nvm"
[ -s "$NVM_DIR/nvm.sh" ] && \. "$NVM_DIR/nvm.sh"

nvm install 22
nvm use 22

node -v
npm -v

npm install -g @github/copilot
#On first run, it’ll ask to trust the folder, and then you can use /login to authenticate. 
copilot 

#for antigravity cli
curl -fsSL https://raw.githubusercontent.com/antigravity-ai/install/main/install.sh | bash

# --- 9. Zenoh Router Systemd Service Setup ---
echo "--- 9. Setting up Zenoh Router Systemd User Service ---"
mkdir -p "$HOME/.config/systemd/user"

if [ "$ARCH" = "aarch64" ]; then
    # Jetson acts as the listener/host router, no need to connect to another router
    cat << EOF > "$HOME/.config/systemd/user/zenoh-router.service"
[Unit]
Description=Zenoh Router for ROS 2 (rmw_zenoh_cpp)
After=network.target

[Service]
ExecStart=/bin/bash -c "source /home/jeevan/Jetcar/.venv/bin/activate && ros2 run rmw_zenoh_cpp rmw_zenohd"
Restart=always
RestartSec=3

[Install]
WantedBy=default.target
EOF
else
    # PC acts as the client router and connects directly to the Jetson router
    cat << EOF > "$HOME/.config/systemd/user/zenoh-router.service"
[Unit]
Description=Zenoh Router for ROS 2 (rmw_zenoh_cpp)
After=network.target

[Service]
ExecStart=/bin/bash -c "source \$HOME/Jetcar/.venv/bin/activate && \
                        export ZENOH_CONFIG_OVERRIDE='connect/endpoints=[\"tcp/${JETSON_IP}:7447\"]' && \
                        ros2 run rmw_zenoh_cpp rmw_zenohd"

Restart=always
RestartSec=3

[Install]
WantedBy=default.target
EOF
fi

# reload and enable the service
systemctl --user daemon-reload
systemctl --user enable --now zenoh-router.service
echo "Zenoh router service configured and enabled to start on system boot/user login."

# To run, at prompt
# agy


