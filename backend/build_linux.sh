#!/bin/bash
set -e

echo "Building CafeInternetManager Backend for Linux..."

# Create build directory
mkdir -p build_linux
cd build_linux

# Configure CMake
# PLATFORM_LINUX is automatically detected by CMakeLists.txt on Linux
cmake .. -DCMAKE_BUILD_TYPE=Release

# Build
make -j$(nproc) BackendServer

echo "Build complete!"
echo "To run the server:"
echo "  cd build_linux"
echo "  ./BackendServer [optional_port]"
