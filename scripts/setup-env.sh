#!/bin/bash
# setup ROS2 environment, NoMachine, and other developer necessities.

# --- Configuration Variables ---
USERNAME="xyz" # CHANGE THIS to your computer's username
EMAIL="xyz@gmail.com" # CHANGE THIS
GIT_USER="xyz" # CHANGE THIS
ROS2_DISTRO="humble" # Assuming you're on a 22.04 base. If 24.04, change to "jazzy"

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
echo "--- 6. Setting up ROS 2 Humble Repository and Installing Packages ---"

# --- 1. ROS 2 Humble Installation Procedure (Lighter Version) ---
echo "--- 1. Setting up ROS 2 Humble Repository and Installing ros-base ---"

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

# Update package cache
sudo apt update

# Install the ROS 2 Humble base package (Minimal core tools and communication)
sudo apt install ros-$ROS2_DISTRO-ros-base -y

#install rosbridge_server to cnnect foxglove visualisation
sudo apt install ros-$ROS2_DISTRO-foxglove-bridge


# Install development tools (colcon, etc.)
sudo apt install python3-colcon-common-extensions ros-dev-tools -y

# Automatically source ROS 2 upon every new terminal login
echo "source /opt/ros/$ROS2_DISTRO/setup.bash" >> ~/.bashrc

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

# 2. Install the full JetPack SDK component list
sudo apt install nvidia-jetpack

sudo apt install libopencv-dev python3-opencv -y
#Then, in the venv you use, run this in terminal.
# Set the custom repository index for JetPack 6.2 (CUDA 12.6)
export PIP_INDEX_URL=https://pypi.jetson-ai-lab.io/jp6/cu126 

# Install the optimized wheels via pip
pip install opencv-python-headless

Also add the lines to .venv/bin/activate at the end 
export LD_LIBRARY_PATH="/usr/lib/aarch64-linux-gnu/tegra:$LD_LIBRARY_PATH"

#For docker...tbc

