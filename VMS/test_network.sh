#!/bin/bash

# Test network configuration for VMS

echo "Testing network configuration for Video Management System..."
echo "=========================================================="

# Test 1: Check if we can bind to localhost
echo "1. Testing localhost binding..."
if timeout 2 bash -c 'exec 3<>/dev/tcp/127.0.0.1/8080' 2>/dev/null; then
    echo "   ❌ Port 8080 is already in use on localhost"
    exec 3<&-
    exec 3>&-
else
    echo "   ✅ Port 8080 is available on localhost"
fi

# Test 2: Check network interfaces
echo ""
echo "2. Checking network interfaces..."
if command -v ip &> /dev/null; then
    echo "   Available interfaces:"
    ip addr show | grep -E "^[0-9]+:" | sed 's/^/   /'
elif command -v ifconfig &> /dev/null; then
    echo "   Available interfaces:"
    ifconfig | grep -E "^[a-zA-Z]" | sed 's/^/   /'
else
    echo "   ❌ Cannot detect network interfaces"
fi

# Test 3: Check IP addresses
echo ""
echo "3. Checking IP addresses..."
if command -v ip &> /dev/null; then
    IP=$(ip route get 8.8.8.8 2>/dev/null | grep -oP 'src \K\S+' | head -1)
elif command -v ifconfig &> /dev/null; then
    IP=$(ifconfig 2>/dev/null | grep -oP 'inet \K[0-9.]+' | grep -v '127.0.0.1' | head -1)
else
    IP="127.0.0.1"
fi

if [ -z "$IP" ]; then
    IP="127.0.0.1"
fi

echo "   Primary IP: $IP"

# Test 4: Check if we can bind to all interfaces
echo ""
echo "4. Testing network binding capabilities..."
if [ "$EUID" -eq 0 ]; then
    echo "   ✅ Running as root - can bind to privileged ports"
else
    echo "   ⚠️  Not running as root - may need sudo for port 8080"
fi

# Test 5: Check GStreamer
echo ""
echo "5. Checking GStreamer installation..."
if pkg-config --exists gstreamer-1.0; then
    echo "   ✅ GStreamer 1.0 found"
    pkg-config --modversion gstreamer-1.0 | sed 's/^/   Version: /'
else
    echo "   ❌ GStreamer 1.0 not found"
fi

# Test 6: Check OpenSSL
echo ""
echo "6. Checking OpenSSL installation..."
if pkg-config --exists openssl; then
    echo "   ✅ OpenSSL found"
    pkg-config --modversion openssl | sed 's/^/   Version: /'
else
    echo "   ❌ OpenSSL not found"
fi

echo ""
echo "=========================================================="
echo "Recommendations:"
echo ""

if [ "$IP" = "127.0.0.1" ]; then
    echo "⚠️  Only localhost IP detected. VMS will only be accessible locally."
    echo "   To make it accessible from other devices:"
    echo "   1. Configure a network interface with a static IP"
    echo "   2. Or modify the code to use 0.0.0.0 for all interfaces"
else
    echo "✅ Network IP detected: $IP"
    echo "   VMS will be accessible at: http://$IP:8080"
fi

echo ""
echo "To start VMS:"
if [ "$EUID" -eq 0 ]; then
    echo "   ./start_vms.sh"
else
    echo "   sudo ./start_vms.sh"
fi

