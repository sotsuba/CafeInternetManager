#!/bin/bash
set -e

# Colors
GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m' # No Color

echo -e "${GREEN}[Setup] Starting Universal Agent Setup for Linux...${NC}"

# 1. Install Dependencies
echo -e "${GREEN}[Setup] Installing System Dependencies...${NC}"
if command -v apt-get &> /dev/null; then
    sudo apt-get update
    sudo apt-get install -y build-essential cmake g++ ffmpeg libx11-dev libxtst-dev libevdev-dev x11-xserver-utils
elif command -v dnf &> /dev/null; then
    sudo dnf install -y gcc-c++ cmake ffmpeg libX11-devel libXtst-devel libevdev-devel xrandr
elif command -v pacman &> /dev/null; then
    sudo pacman -S --noconfirm base-devel cmake ffmpeg libx11 libxtst libevdev xorg-xrandr
else
    echo -e "${RED}[Error] Unsupported Package Manager. Please install cmake, g++, ffmpeg manually.${NC}"
    exit 1
fi

# 2. Check Permissions (Keylogger needs access to /dev/input)
echo -e "${GREEN}[Setup] Checking Permissions...${NC}"
if [ ! -r /dev/input/event0 ]; then
    echo -e "${RED}[Warning] Current user cannot read /dev/input devices.${NC}"
    echo "Attempting to fix by adding user to 'input' group..."
    sudo usermod -aG input $USER || true
    echo "You may need to logout and login again for group changes to take effect."
fi

# 3. Build Project
echo -e "${GREEN}[Setup] Building Backend...${NC}"
mkdir -p build
cd build
cmake ..
make -j$(nproc)

echo -e "${GREEN}[Setup] Build Complete!${NC}"
echo -e "Run the agent with: ${GREEN}./build/BackendServer${NC}"
