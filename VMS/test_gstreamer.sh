#!/bin/bash

# Test GStreamer installation and basic functionality

echo "Testing GStreamer Installation..."
echo "================================="

# Test 1: Check GStreamer version
echo "1. GStreamer version:"
gst-launch-1.0 --version 2>/dev/null || echo "   ❌ gst-launch-1.0 not found"

# Test 2: Check available plugins
echo ""
echo "2. Checking essential plugins..."
plugins=("videotestsrc" "videoconvert" "x264enc" "rtph264pay" "udpsink" "autovideosink")

for plugin in "${plugins[@]}"; do
    if gst-inspect-1.0 "$plugin" >/dev/null 2>&1; then
        echo "   ✅ $plugin plugin available"
    else
        echo "   ❌ $plugin plugin NOT available"
    fi
done

# Test 3: Test basic pipeline
echo ""
echo "3. Testing basic video pipeline..."
echo "   Running: gst-launch-1.0 videotestsrc num-buffers=10 ! autovideosink"

timeout 5s gst-launch-1.0 videotestsrc num-buffers=10 ! autovideosink 2>&1 | head -5
if [ ${PIPESTATUS[0]} -eq 0 ]; then
    echo "   ✅ Basic video pipeline works"
else
    echo "   ❌ Basic video pipeline failed"
fi

# Test 4: Test UDP streaming pipeline
echo ""
echo "4. Testing UDP streaming pipeline..."
echo "   This will run for 3 seconds..."

# Start UDP sink in background
timeout 3s gst-launch-1.0 videotestsrc num-buffers=90 ! videoconvert ! x264enc ! rtph264pay ! udpsink host=127.0.0.1 port=9999 &
GST_PID=$!

# Check if port is listening
sleep 1
if netstat -tuln 2>/dev/null | grep -q ":9999 "; then
    echo "   ✅ UDP streaming pipeline works (port 9999 active)"
else
    echo "   ❌ UDP streaming pipeline failed (port 9999 not active)"
fi

# Clean up
wait $GST_PID 2>/dev/null

echo ""
echo "================================="
echo "GStreamer Test Complete"
echo ""
echo "If any tests failed, you may need to install additional GStreamer plugins:"
echo "  Ubuntu/Debian: sudo apt-get install gstreamer1.0-plugins-*"
echo "  CentOS/RHEL: sudo yum install gstreamer1-plugins-*"

