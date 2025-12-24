#!/bin/bash

# SafeSchool - Universal Linux Setup Script
# Distro-agnostic setup for CafeInternetManager Backend

echo "🚀 Starting SafeSchool Environment Setup..."

# 1. Detect Package Manager
HAS_APT=$(which apt-get 2>/dev/null)
HAS_DNF=$(which dnf 2>/dev/null)
HAS_PACMAN=$(which pacman 2>/dev/null)

install_deps() {
    echo "📦 Installing Dependencies..."
    if [ ! -z "$HAS_APT" ]; then
        echo "Detected: Debian/Ubuntu (apt)"
        sudo apt-get update
        sudo apt-get install -y build-essential cmake ffmpeg scrot x11-utils procps libssl-dev
    elif [ ! -z "$HAS_DNF" ]; then
        echo "Detected: Fedora/RHEL (dnf)"
        sudo dnf install -y gcc-c++ cmake ffmpeg scrot xrandr procps-ng openssl-devel
    elif [ ! -z "$HAS_PACMAN" ]; then
        echo "Detected: Arch Linux (pacman)"
        sudo pacman -Sy --noconfirm base-devel cmake ffmpeg scrot xorg-xrandr procps-ng openssl
    else
        echo "⚠️  Unknown Package Manager. Please manually install: ffmpeg, scrot, xrandr, cmake, g++"
    fi
}

# 2. Keylogger Permissions
setup_permissions() {
    echo "🔑 Configuring Permissions for Keylogger..."
    # Check if 'input' group exists
    if getent group input > /dev/null; then
        echo "   Group 'input' exists."
    else
        echo "   Creating group 'input'..."
        sudo groupadd input
    fi

    # Add current user to input group
    if groups $USER | grep &>/dev/null "\binput\b"; then
        echo "   User '$USER' is already in 'input' group."
    else
        echo "   Adding '$USER' to 'input' group..."
        sudo usermod -aG input $USER
        echo "⚠️  YOU MUST LOGOUT AND LOGIN AGAIN for group changes to take effect!"
    fi

    # Permissions for uinput (optional but good practice)
    echo "   Setting udev rule for /dev/input/event* access..."
    echo 'KERNEL=="event*", NAME="input/%k", MODE="660", GROUP="input"' | sudo tee /etc/udev/rules.d/99-input.rules > /dev/null
    sudo udevadm control --reload-rules 2>/dev/null || echo "   (Skipped udev reload - manual restart might be needed)"
}

# 3. Environment Check
check_env() {
    echo "🖥️  Checking Session Type..."
    if [ "$XDG_SESSION_TYPE" == "wayland" ]; then
        echo "⚠️  WARNING: You are running WAYLAND."
        echo "   Screen Streaming relies on X11 (x11grab)."
        echo "   Please log out and select 'Ubuntu on Xorg' or 'GhostBSD (X11)' or similar at the login screen."
        echo "   Otherwise, screen streaming will show a black screen."
    else
        echo "✅ Detected X11 Session. Streaming should work."
    fi
}

# Execute
install_deps
setup_permissions
check_env

echo "✅ Setup Complete. Reboot or Re-login recommended."
