#!/bin/bash
# ============================================================================
# CafeInternetManager - Gateway Run Script (Auto-Setup)
# This script ensures dependencies are installed, builds the gateway,
# and starts it in discovery mode.
# ============================================================================

set -e

# Colors for better output
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}=============================================="
echo "   CafeInternetManager - Gateway Launcher"
echo -e "==============================================${NC}"

# 1. Run the internal setup script which handles dependencies and build
if [ ! -f "gateway/setup_gateway_linux.sh" ]; then
    echo "ERROR: Could not find gateway/setup_gateway_linux.sh"
    exit 1
fi

chmod +x gateway/setup_gateway_linux.sh

echo -e "${YELLOW}[Step 1/2] Running setup and build...${NC}"
# Use sudo only for the parts that need it inside the setup script
# The setup script itself uses sudo for apt-get/dnf/pacman
cd gateway && ./setup_gateway_linux.sh
cd ..

echo -e "${GREEN}Gateway execution finished.${NC}"
