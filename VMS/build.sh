#!/bin/bash

# Video Management System Build Script
# For embedded Linux systems

set -e

echo "Building Video Management System..."

# Check if running on Linux
if [[ "$OSTYPE" != "linux-gnu"* ]]; then
    echo "Error: This build script is designed for Linux systems"
    exit 1
fi

# Check for required dependencies
echo "Checking dependencies..."

# Check for GStreamer
if ! pkg-config --exists gstreamer-1.0; then
    echo "Error: GStreamer 1.0 not found. Please install:"
    echo "  Ubuntu/Debian: sudo apt-get install libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev"
    echo "  CentOS/RHEL: sudo yum install gstreamer1-devel gstreamer1-plugins-base-devel"
    exit 1
fi

# Check for OpenSSL
if ! pkg-config --exists openssl; then
    echo "Error: OpenSSL not found. Please install:"
    echo "  Ubuntu/Debian: sudo apt-get install libssl-dev"
    echo "  CentOS/RHEL: sudo yum install openssl-devel"
    exit 1
fi

# Check for CMake
if ! command -v cmake &> /dev/null; then
    echo "Error: CMake not found. Please install:"
    echo "  Ubuntu/Debian: sudo apt-get install cmake"
    echo "  CentOS/RHEL: sudo yum install cmake"
    exit 1
fi

# Check for make
if ! command -v make &> /dev/null; then
    echo "Error: make not found. Please install build-essential"
    exit 1
fi

echo "All dependencies found!"

# Create build directory
mkdir -p build
cd build

# Configure with CMake
echo "Configuring with CMake..."
cmake .. -DCMAKE_BUILD_TYPE=Release

# Build the project
echo "Building..."
make -j$(nproc)

echo "Build completed successfully!"
echo ""
echo "To run the application:"
echo "  sudo ./vms"
echo ""
echo "Note: Running with sudo may be required for network binding on port 8080"
echo "The web interface will be available at: http://192.168.1.1:8080"

