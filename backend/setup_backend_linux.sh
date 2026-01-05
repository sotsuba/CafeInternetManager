#!/bin/bash

# Exit on error
set -e

echo "╔══════════════════════════════════════════════════════════════╗"
echo "║          Backend Server Setup (Linux Native)                 ║"
echo "╚══════════════════════════════════════════════════════════════╝"

# 1. Detect Distro and Install Dependencies
echo "[Setup] Checking dependencies..."
if [ -f /etc/os-release ]; then
    . /etc/os-release
    OS=$NAME
elif type lsb_release >/dev/null 2>&1; then
    OS=$(lsb_release -si)
else
    OS=$(uname -s)
fi

echo "[Setup] OS Detected: $OS"

if [[ "$OS" == *"Ubuntu"* ]] || [[ "$OS" == *"Debian"* ]] || [[ "$OS" == *"Kali"* ]]; then
    sudo apt-get update > /dev/null
    # Install Build Tools + X11/Xtst (for Input Injection) + SSL
    sudo apt-get install -y cmake build-essential libssl-dev libx11-dev libxtst-dev libuinput-dev > /dev/null
elif [[ "$OS" == *"Fedora"* ]] || [[ "$OS" == *"CentOS"* ]]; then
    sudo dnf install -y cmake gcc-c++ openssl-devel libX11-devel libXtst-devel
elif [[ "$OS" == *"Arch"* ]] || [[ "$OS" == *"Manjaro"* ]]; then
    sudo pacman -S --needed cmake base-devel openssl libx11 libxtst
fi

# 2. Configure Firewall (Allow 9090 for Gateway Connection)
echo -e "\n[Setup] Configuring Firewall..."
if command -v ufw >/dev/null; then
    echo "  -> Allowing TCP 9090 (Backend Listener)"
    sudo ufw allow 9090/tcp > /dev/null
    # Outbound UDP 9999 is allowed by default, but nice to be sure if paranoid
    # sudo ufw allow out 9999/udp
    echo "[Setup] Firewall rules updated."
else
    echo "[Setup] UFW not found. Ensure TCP port 9090 is open."
fi

# 3. Build
echo -e "\n[Setup] Building Backend..."
rm -rf build
mkdir -p build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release > /dev/null
make -j$(nproc)

# 4. Run
echo -e "\n[Setup] Build complete. Starting Backend Server..."
SERVER_BIN="./BackendServer"

if [ -f "$SERVER_BIN" ]; then
    echo "[Info] Backend listening on 9090 (TCP) and broadcasting on 9999 (UDP)"
    # Enable uinput permissions if needed? (Requires user in input group or sudo)
    # We will try running as invoking user. If input injection fails, user needs sudo or group config.

    # Check for uinput access
    if [ ! -w /dev/uinput ]; then
        echo "[Warning] /dev/uinput is not writable. Input Injection (Keyboard/Mouse) might fail."
        echo "          You can run with 'sudo' or add logic to add user to 'input' group."
    fi

    "$SERVER_BIN" 9090
else
    echo "[Error] Binary not found!"
    exit 1
fi
