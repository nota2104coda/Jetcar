#!/bin/bash
# setup ROS2 environment, NoMachine, and other developer necessities.

# --- Configuration Variables ---
USERNAME="xyz" # CHANGE THIS to your computer's username
EMAIL="xyz@gmail.com" # CHANGE THIS
GIT_USER="xyz" # CHANGE THIS
ROS2_DISTRO="jazzy" # Assuming you're on a 22.04 base. If 24.04, change to "jazzy"
export ARCH=$(uname -m)
echo $ARCH


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
sudo ufw enable
sudo ufw allow 4000/tcp # Allow NoMachine port

# --- 3. Remote Desktop (XFCE4 & NoMachine) Setup ---
# 3.1 Download and install NoMachine for ARM64 (for RPi5)
cd /tmp
wget https://www.nomachine.com/free/arm/v8/deb -O nomachine_arm64.deb
sudo dpkg -i nomachine_arm64.deb
rm nomachine_arm64.deb

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

# --- 6. ROS 2 Humble Installation Procedure ---
echo "--- 6. Setting up ROS 2 Humble/Jazzy Repository and Installing Packages ---"

# --- 1. ROS 2 Humble Installation Procedure (Lighter Version) ---
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
# Install slam_toolbox for mapping and localization
sudo apt install ros-$ROS2_DISTRO-slam-toolbox -y
ARCH=$(uname -m)


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

# --- 8. Environment Configuration (.bashrc) ---
echo "--- 8. Configuring .bashrc and User Environment ---"

BASHRC="$HOME/.bashrc"

if ! grep -q "# Jetcar Environment Configuration" "$BASHRC"; then
cat << EOF >> "$BASHRC"

# Jetcar Environment Configuration
source /opt/ros/$ROS2_DISTRO/setup.bash
export RMW_IMPLEMENTATION=rmw_cyclonedds_cpp

# Jetcar Workspace Aliases
alias sdev='source install/setup.bash && echo "sourced install/setup.bash" '

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

alias rlgaz='ros2 launch jetcar_sim gazebo.launch.py'

cb() {
   colcon build --symlink-install "\$@"
}

alias sros='source /opt/ros/$ROS2_DISTRO/setup.bash'
EOF
    echo "Shortcuts and ROS configuration added to ~/.bashrc"
fi

#To run Gemini or Copilot CLI
#---8. Install copilot CLI and gemini CLI
sudo apt install curl
curl -o- https://raw.githubusercontent.com/nvm-sh/nvm/v0.40.1/install.sh | bash
source ~/.bashrc
nvm install 22
nvm use 22

node -v
npm -v

npm install -g @github/copilot
#On first run, it’ll ask to trust the folder, and then you can use /login to authenticate. 
copilot 

#for gemini cli
npm install -g @google/gemini-cli

#To run, at prompt
#>gemini

#for JETSON ONLY: Install JetPack SDK and OpenCV
# 1. Update your package lists to ensure the Jetpack repositories are current
sudo apt update
#for adding this user in the docker root users group. Once added, your user (and thus # yourPython script running as that user) can execute docker run commands without needing # sudo.
sudo usermod -aG docker $USERNAME

#For docker...tbc

