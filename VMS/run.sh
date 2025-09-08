#!/bin/bash

# Video Management System Run Script
# Provides easy startup with proper configuration

set -e

echo "Starting Video Management System..."

# Check if running as root (required for network binding)
if [ "$EUID" -ne 0 ]; then
    echo "Warning: Not running as root. Network binding may fail."
    echo "Consider running with: sudo $0"
    echo ""
fi

# Check if the binary exists
if [ ! -f "./build/vms" ]; then
    echo "Error: VMS binary not found. Please build the application first:"
    echo "  ./build.sh"
    exit 1
fi

# Check if port 8080 is available
if netstat -tuln 2>/dev/null | grep -q ":8080 "; then
    echo "Warning: Port 8080 is already in use."
    echo "Please stop the conflicting service or modify the port in the source code."
    echo ""
fi

# Set up environment variables
export GST_DEBUG=2
export GST_DEBUG_NO_COLOR=1

# Create logs directory if it doesn't exist
mkdir -p logs

# Start the application
echo "Starting VMS on 192.168.1.1:8080..."
echo "Web interface: http://192.168.1.1:8080"
echo "Press Ctrl+C to stop"
echo ""

# Run with logging
./build/vms 2>&1 | tee logs/vms-$(date +%Y%m%d-%H%M%S).log

