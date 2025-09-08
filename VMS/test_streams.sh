#!/bin/bash

# Test VMS streams and UDP ports

echo "Testing VMS Streams..."
echo "====================="

# Test 1: Check if VMS is running
echo "1. Checking if VMS is running..."
if curl -s http://127.0.0.1:8080/api/streams >/dev/null 2>&1; then
    echo "   ✅ VMS is running"
else
    echo "   ❌ VMS is not running. Start it with: ./build/vms"
    exit 1
fi

# Test 2: Check stream status via API
echo ""
echo "2. Checking stream status via API..."
API_RESPONSE=$(curl -s http://127.0.0.1:8080/api/streams)
echo "   API Response: $API_RESPONSE"

# Test 3: Check UDP ports
echo ""
echo "3. Checking UDP ports..."
ACTIVE_PORTS=0
for port in {8081..8088}; do
    if netstat -tuln 2>/dev/null | grep -q ":$port "; then
        echo "   ✅ Port $port is active"
        ACTIVE_PORTS=$((ACTIVE_PORTS + 1))
    else
        echo "   ❌ Port $port is not active"
    fi
done

# Test 4: Try to connect to a UDP port
echo ""
echo "4. Testing UDP connection..."
if command -v nc &> /dev/null; then
    echo "   Testing connection to port 8081..."
    timeout 2s nc -u -z 127.0.0.1 8081 2>/dev/null
    if [ $? -eq 0 ]; then
        echo "   ✅ UDP port 8081 is accepting connections"
    else
        echo "   ❌ UDP port 8081 is not accepting connections"
    fi
else
    echo "   ⚠️  netcat not available, skipping UDP connection test"
fi

# Test 5: Check GStreamer processes
echo ""
echo "5. Checking GStreamer processes..."
GST_PROCESSES=$(ps aux | grep -c gst-launch)
if [ $GST_PROCESSES -gt 1 ]; then
    echo "   ✅ GStreamer processes found: $((GST_PROCESSES - 1))"
else
    echo "   ❌ No GStreamer processes found"
fi

# Test 6: Check system resources
echo ""
echo "6. Checking system resources..."
echo "   CPU Usage: $(top -bn1 | grep "Cpu(s)" | awk '{print $2}' | cut -d'%' -f1)%"
echo "   Memory Usage: $(free | grep Mem | awk '{printf "%.1f%%", $3/$2 * 100.0}')"

echo ""
echo "Summary:"
echo "========"
echo "Active UDP Ports: $ACTIVE_PORTS/8"
echo "GStreamer Processes: $((GST_PROCESSES - 1))"

if [ $ACTIVE_PORTS -eq 8 ]; then
    echo ""
    echo "🎉 All streams are working correctly!"
    echo "   You can now access the web interface at: http://127.0.0.1:8080"
    echo "   Click on any stream preview to view it"
else
    echo ""
    echo "⚠️  Some streams are not working properly."
    echo "   Try running: ./test_gstreamer.sh"
    echo "   Check VMS logs for error messages"
fi

