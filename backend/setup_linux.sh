#!/bin/bash
# =============================================================================
# CafeManager Backend - Linux Setup Script
# =============================================================================
# Supports: Ubuntu/Debian, Fedora, Arch Linux, openSUSE
# Usage: sudo ./setup_linux.sh
# =============================================================================

set -e

# Colors
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

log()   { echo -e "${GREEN}[✓]${NC} $1"; }
warn()  { echo -e "${YELLOW}[!]${NC} $1"; }
error() { echo -e "${RED}[✗]${NC} $1"; exit 1; }

# =============================================================================
# Check Root
# =============================================================================
if [ "$EUID" -ne 0 ]; then
    error "Please run as root: sudo $0"
fi

REAL_USER="${SUDO_USER:-$USER}"
log "Running as root, real user: $REAL_USER"

# =============================================================================
# Detect Distro
# =============================================================================
detect_distro() {
    if [ -f /etc/os-release ]; then
        . /etc/os-release
        DISTRO="$ID"
        DISTRO_LIKE="$ID_LIKE"
    elif command -v lsb_release &>/dev/null; then
        DISTRO=$(lsb_release -is | tr '[:upper:]' '[:lower:]')
    else
        error "Cannot detect distro"
    fi
    log "Detected distro: $DISTRO"
}

# =============================================================================
# Install Dependencies
# =============================================================================
install_deps() {
    log "Installing dependencies..."

    case "$DISTRO" in
        ubuntu|debian|linuxmint|pop)
            apt-get update -qq
            apt-get install -y \
                build-essential cmake g++ ffmpeg \
                libx11-dev libxtst-dev libevdev-dev \
                x11-xserver-utils pkg-config
            ;;
        fedora)
            dnf install -y \
                gcc-c++ cmake ffmpeg \
                libX11-devel libXtst-devel libevdev-devel \
                xrandr
            ;;
        arch|manjaro|endeavouros)
            pacman -Syu --noconfirm
            pacman -S --noconfirm --needed \
                base-devel cmake ffmpeg \
                libx11 libxtst libevdev \
                xorg-xrandr
            ;;
        opensuse*|suse)
            zypper install -y \
                gcc-c++ cmake ffmpeg \
                libX11-devel libXtst-devel libevdev-devel
            ;;
        *)
            # Try to detect by ID_LIKE
            if [[ "$DISTRO_LIKE" == *"debian"* ]]; then
                apt-get update -qq
                apt-get install -y build-essential cmake g++ ffmpeg \
                    libx11-dev libxtst-dev libevdev-dev x11-xserver-utils
            elif [[ "$DISTRO_LIKE" == *"arch"* ]]; then
                pacman -S --noconfirm --needed base-devel cmake ffmpeg \
                    libx11 libxtst libevdev xorg-xrandr
            elif [[ "$DISTRO_LIKE" == *"fedora"* ]] || [[ "$DISTRO_LIKE" == *"rhel"* ]]; then
                dnf install -y gcc-c++ cmake ffmpeg \
                    libX11-devel libXtst-devel libevdev-devel
            else
                error "Unsupported distro: $DISTRO. Please install manually: cmake g++ ffmpeg libx11-dev libxtst-dev libevdev-dev"
            fi
            ;;
    esac

    log "Dependencies installed!"
}

# =============================================================================
# Install Wayland Tools (Optional)
# =============================================================================
install_wayland_tools() {
    if [ "$XDG_SESSION_TYPE" = "wayland" ] || [ -n "$WAYLAND_DISPLAY" ]; then
        warn "Wayland detected, installing screen capture tools..."

        case "$DISTRO" in
            ubuntu|debian|linuxmint|pop)
                apt-get install -y grim wl-clipboard || true
                # wl-screenrec might need to be built from source
                ;;
            fedora)
                dnf install -y grim wl-clipboard || true
                ;;
            arch|manjaro|endeavouros)
                pacman -S --noconfirm --needed grim wl-clipboard wf-recorder || true
                ;;
        esac
    fi
}

# =============================================================================
# Setup Permissions
# =============================================================================
setup_permissions() {
    log "Setting up permissions..."

    # Add user to input group (for keylogger)
    if ! groups "$REAL_USER" | grep -q '\binput\b'; then
        usermod -aG input "$REAL_USER"
        warn "Added $REAL_USER to 'input' group (logout required for effect)"
    fi

    # Setup uinput permissions (for input injection)
    if [ ! -f /etc/udev/rules.d/99-uinput.rules ]; then
        echo 'KERNEL=="uinput", MODE="0666", GROUP="input"' > /etc/udev/rules.d/99-uinput.rules
        udevadm control --reload-rules
        udevadm trigger
        log "uinput permissions configured"
    fi

    # Temporary fix for current session
    chmod 666 /dev/uinput 2>/dev/null || true
}

# =============================================================================
# Build Project
# =============================================================================
build_project() {
    log "Building backend..."

    SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
    cd "$SCRIPT_DIR"

    # Build as real user, not root
    sudo -u "$REAL_USER" mkdir -p build
    cd build
    sudo -u "$REAL_USER" cmake ..
    sudo -u "$REAL_USER" make -j$(nproc)

    log "Build complete!"
}

# =============================================================================
# Main
# =============================================================================
main() {
    echo ""
    echo "╔══════════════════════════════════════════════════════════════╗"
    echo "║       CafeManager Backend - Linux Setup                      ║"
    echo "╚══════════════════════════════════════════════════════════════╝"
    echo ""

    detect_distro
    install_deps
    install_wayland_tools
    setup_permissions
    build_project

    echo ""
    log "Setup complete!"
    echo ""
    echo -e "  Run the backend with: ${GREEN}./build/BackendServer${NC}"
    echo ""
    warn "You may need to logout and login for group changes to take effect."
    echo ""
}

main "$@"
