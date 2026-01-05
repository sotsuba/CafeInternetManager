#!/bin/bash
# ============================================================================
# CafeInternetManager - Linux Setup Script
# Run this ONCE with sudo to configure all permissions
# Usage: sudo ./setup_linux.sh
# ============================================================================

set -e

echo "=============================================="
echo " CafeInternetManager - Linux One-Time Setup"
echo "=============================================="

# Get the actual user (not root)
ACTUAL_USER="${SUDO_USER:-$USER}"

if [ "$EUID" -ne 0 ]; then
    echo "ERROR: This script must be run with sudo"
    echo "Usage: sudo ./setup_linux.sh"
    exit 1
fi

echo ""
echo "[1/5] Adding user '$ACTUAL_USER' to 'input' group (for keylogger)..."
usermod -aG input "$ACTUAL_USER"
echo "      Done."

echo ""
echo "[2/5] Adding user '$ACTUAL_USER' to 'video' group (for webcam)..."
usermod -aG video "$ACTUAL_USER"
echo "      Done."

echo ""
echo "[3/5] Installing required packages..."
if command -v apt-get &> /dev/null; then
    apt-get update -qq
    apt-get install -y -qq ffmpeg libx11-dev libxtst-dev build-essential cmake
elif command -v dnf &> /dev/null; then
    dnf install -y -q ffmpeg libX11-devel libXtst-devel gcc-c++ cmake
elif command -v pacman &> /dev/null; then
    pacman -Sy --noconfirm ffmpeg libx11 libxtst base-devel cmake
else
    echo "      WARNING: Unknown package manager. Please install manually:"
    echo "      - ffmpeg"
    echo "      - libx11-dev / libX11-devel"
    echo "      - libxtst-dev / libXtst-devel"
    echo "      - cmake"
    echo "      - build-essential / base-devel"
fi
echo "      Done."

echo ""
echo "[4/5] Setting permissions for /dev/input/event* (for keylogger)..."
# Create udev rule for persistent permissions
cat > /etc/udev/rules.d/99-cafe-input.rules << 'EOF'
# CafeInternetManager: Allow input group to read keyboard events
KERNEL=="event*", SUBSYSTEM=="input", GROUP="input", MODE="0640"
EOF
udevadm control --reload-rules
udevadm trigger
echo "      Done."

echo ""
echo "[5/5] Setting video device permissions..."
# Create udev rule for webcam
cat > /etc/udev/rules.d/99-cafe-video.rules << 'EOF'
# CafeInternetManager: Allow video group to access webcam
KERNEL=="video*", SUBSYSTEM=="video4linux", GROUP="video", MODE="0660"
EOF
udevadm control --reload-rules
udevadm trigger
echo "      Done."

echo ""
echo "=============================================="
echo " Setup Complete!"
echo "=============================================="
echo ""
echo "IMPORTANT: You must log out and log back in"
echo "for group changes to take effect."
echo ""
echo "After logging back in, run:"
echo "  cd backend"
echo "  ./build_linux.sh"
echo "  cd build_linux"
echo "  ./BackendServer"
echo ""
