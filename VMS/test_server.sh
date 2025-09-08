#!/bin/bash

# Test script to verify VMS server is working

echo "Testing VMS Server..."
echo "===================="

# Test 1: Check if server is responding
echo "1. Testing HTTP server response..."
if curl -s -o /dev/null -w "%{http_code}" http://127.0.0.1:8080/ | grep -q "200"; then
    echo "   ✅ HTTP server is responding"
else
    echo "   ❌ HTTP server is not responding"
    echo "   Make sure VMS is running: ./build/vms"
    exit 1
fi

# Test 2: Test API endpoint
echo ""
echo "2. Testing API endpoint..."
API_RESPONSE=$(curl -s http://127.0.0.1:8080/api/streams)
if echo "$API_RESPONSE" | grep -q "streams"; then
    echo "   ✅ API endpoint is working"
    echo "   Response: $API_RESPONSE"
else
    echo "   ❌ API endpoint is not working"
    echo "   Response: $API_RESPONSE"
fi

# Test 3: Test web interface
echo ""
echo "3. Testing web interface..."
WEB_RESPONSE=$(curl -s -o /dev/null -w "%{http_code}" http://127.0.0.1:8080/)
if [ "$WEB_RESPONSE" = "200" ]; then
    echo "   ✅ Web interface is accessible"
else
    echo "   ❌ Web interface is not accessible (HTTP $WEB_RESPONSE)"
fi

# Test 4: Check stream ports
echo ""
echo "4. Testing stream ports..."
STREAM_PORTS_ACTIVE=0
for port in {8081..8088}; do
    if netstat -tuln 2>/dev/null | grep -q ":$port "; then
        echo "   ✅ Stream port $port is active"
        STREAM_PORTS_ACTIVE=$((STREAM_PORTS_ACTIVE + 1))
    else
        echo "   ❌ Stream port $port is not active"
    fi
done

echo ""
echo "Summary:"
echo "========="
echo "HTTP Server: $([ "$WEB_RESPONSE" = "200" ] && echo "✅ Working" || echo "❌ Not working")"
echo "API Endpoint: $([ -n "$API_RESPONSE" ] && echo "✅ Working" || echo "❌ Not working")"
echo "Active Streams: $STREAM_PORTS_ACTIVE/8"

if [ "$STREAM_PORTS_ACTIVE" -eq 8 ] && [ "$WEB_RESPONSE" = "200" ]; then
    echo ""
    echo "🎉 VMS is working correctly!"
    echo "   Web Interface: http://127.0.0.1:8080"
    echo "   API Endpoint: http://127.0.0.1:8080/api/streams"
else
    echo ""
    echo "⚠️  VMS has some issues. Check the logs above."
fi

