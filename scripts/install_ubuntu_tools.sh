#!/bin/bash

# Exit immediately if a command exits with a non-zero status.
set -e

echo "Starting installation of essential tools on Ubuntu 24.04..."

# Update the package list
echo "Updating package list..."
sudo apt update -y

# Install SSH and Git
echo "Installing SSH and Git..."
sudo apt install openssh-server git -y

# Install XRDP
echo "Installing XRDP..."
sudo apt install xrdp -y
sudo systemctl enable --now xrdp


# Install PyCharm Community Edition
echo "Installing PyCharm Community Edition..."
sudo snap install pycharm-community --classic

# Install VS Code
echo "Installing Visual Studio Code..."
wget -qO- https://packages.microsoft.com/keys/microsoft.asc | gpg --dearmor > packages.microsoft.gpg
sudo install -o root -g root -m 644 packages.microsoft.gpg /etc/apt/trusted.gpg.d/
sudo sh -c 'echo "deb [arch=amd64,arm64,armhf] https://packages.microsoft.com/repos/code stable main" > /etc/apt/sources.list.d/vscode.list'
rm -f packages.microsoft.gpg
sudo apt update -y
sudo apt install code -y

# Install Docker
echo "Installing Docker..."
# Add Docker's official GPG key
sudo apt update
sudo apt install ca-certificates curl -y
sudo install -m 0755 -d /etc/apt/keyrings
sudo curl -fsSL https://download.docker.com/linux/ubuntu/gpg -o /etc/apt/keyrings/docker.asc
sudo chmod a+r /etc/apt/keyrings/docker.asc

# Add the Docker repository to Apt sources
echo "deb [arch=$(dpkg --print-architecture) signed-by=/etc/apt/keyrings/docker.asc] https://download.docker.com/linux/ubuntu \
  $(. /etc/os-release && echo "$VERSION_CODENAME") stable" | \
  sudo tee /etc/apt/sources.list.d/docker.list > /dev/null

# Install Docker packages
sudo apt update -y
sudo apt install docker-ce docker-ce-cli containerd.io docker-buildx-plugin docker-compose-plugin -y

# Add the current user to the docker group to run Docker commands without sudo
echo "Adding current user to the 'docker' group..."
sudo usermod -aG docker "$USER"

echo "Installation complete. Please log out and log back in for the changes to the 'docker' group to take effect."