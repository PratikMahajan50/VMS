#!/bin/bash

# Video Management System Dependencies Installation Script
# For embedded Linux systems

set -e

echo "Installing Video Management System dependencies..."

# Detect Linux distribution
if [ -f /etc/os-release ]; then
    . /etc/os-release
    OS=$ID
    VERSION=$VERSION_ID
else
    echo "Error: Cannot detect Linux distribution"
    exit 1
fi

echo "Detected OS: $OS $VERSION"

# Update package lists
echo "Updating package lists..."
if command -v apt-get &> /dev/null; then
    sudo apt-get update
elif command -v yum &> /dev/null; then
    sudo yum update -y
elif command -v dnf &> /dev/null; then
    sudo dnf update -y
else
    echo "Error: Unsupported package manager"
    exit 1
fi

# Install dependencies based on distribution
case $OS in
    ubuntu|debian)
        echo "Installing dependencies for Ubuntu/Debian..."
        sudo apt-get install -y \
            build-essential \
            cmake \
            pkg-config \
            libgstreamer1.0-dev \
            libgstreamer-plugins-base1.0-dev \
            libgstreamer-plugins-bad1.0-dev \
            gstreamer1.0-plugins-base \
            gstreamer1.0-plugins-good \
            gstreamer1.0-plugins-bad \
            gstreamer1.0-plugins-ugly \
            gstreamer1.0-libav \
            gstreamer1.0-tools \
            libssl-dev \
            libglib2.0-dev \
            libxml2-dev
        ;;
    centos|rhel|fedora)
        echo "Installing dependencies for CentOS/RHEL/Fedora..."
        if command -v dnf &> /dev/null; then
            sudo dnf install -y \
                gcc \
                gcc-c++ \
                make \
                cmake \
                pkgconfig \
                gstreamer1-devel \
                gstreamer1-plugins-base-devel \
                gstreamer1-plugins-bad-free-devel \
                gstreamer1-plugins-good \
                gstreamer1-plugins-bad-free \
                gstreamer1-plugins-ugly \
                gstreamer1-libav \
                openssl-devel \
                glib2-devel \
                libxml2-devel
        else
            sudo yum install -y \
                gcc \
                gcc-c++ \
                make \
                cmake \
                pkgconfig \
                gstreamer1-devel \
                gstreamer1-plugins-base-devel \
                gstreamer1-plugins-bad-free-devel \
                gstreamer1-plugins-good \
                gstreamer1-plugins-bad-free \
                gstreamer1-plugins-ugly \
                gstreamer1-libav \
                openssl-devel \
                glib2-devel \
                libxml2-devel
        fi
        ;;
    *)
        echo "Error: Unsupported Linux distribution: $OS"
        echo "Please install the following packages manually:"
        echo "  - build-essential (gcc, g++, make)"
        echo "  - cmake"
        echo "  - pkg-config"
        echo "  - GStreamer 1.0 development libraries"
        echo "  - OpenSSL development libraries"
        echo "  - GLib development libraries"
        exit 1
        ;;
esac

echo ""
echo "Dependencies installed successfully!"
echo ""
echo "Verifying installation..."

# Verify GStreamer installation
if pkg-config --exists gstreamer-1.0; then
    echo "✓ GStreamer 1.0 found"
    pkg-config --modversion gstreamer-1.0
else
    echo "✗ GStreamer 1.0 not found"
fi

# Verify OpenSSL installation
if pkg-config --exists openssl; then
    echo "✓ OpenSSL found"
    pkg-config --modversion openssl
else
    echo "✗ OpenSSL not found"
fi

# Verify CMake installation
if command -v cmake &> /dev/null; then
    echo "✓ CMake found"
    cmake --version | head -n1
else
    echo "✗ CMake not found"
fi

echo ""
echo "You can now run ./build.sh to build the Video Management System"

