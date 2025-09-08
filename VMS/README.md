# Video Management System (VMS)

A high-performance C++ Video Management System designed for embedded Linux systems. This application provides a web-based interface for managing 8 simultaneous 1080P video streams using GStreamer test sources.

## Features

- **8 Simultaneous Video Streams**: Each stream runs at 1080P resolution with 30 FPS
- **Web-Based Interface**: Modern, responsive web UI accessible via browser
- **Real-time Control**: Start/stop individual streams or all streams at once
- **WebSocket Support**: Real-time updates and notifications
- **RESTful API**: Complete API for stream management
- **Embedded Optimized**: Designed for resource-constrained embedded systems
- **GStreamer Integration**: Uses GStreamer for efficient video processing

## System Requirements

### Hardware
- ARM or x86_64 processor
- Minimum 512MB RAM (1GB recommended)
- Network interface with static IP capability
- Storage: 50MB for application + logs

### Software
- Linux kernel 3.10 or later
- GStreamer 1.0 or later
- OpenSSL 1.0 or later
- CMake 3.16 or later
- GCC 7.0 or later

## Quick Start

### 1. Install Dependencies

```bash
# Make the installation script executable
chmod +x install_dependencies.sh

# Run the dependency installer
./install_dependencies.sh
```

### 2. Build the Application

```bash
# Make the build script executable
chmod +x build.sh

# Build the application
./build.sh
```

### 3. Test Network Configuration (Optional)

```bash
# Test network setup and dependencies
chmod +x test_network.sh
./test_network.sh
```

### 4. Run the Application

```bash
# Use the startup script (recommended)
chmod +x start_vms.sh
./start_vms.sh

# Or run directly
cd build
sudo ./vms
```

### 5. Access the Web Interface

The application will automatically detect your system's IP address and display the correct URLs. Typically:

- **Local access**: `http://127.0.0.1:8080`
- **Network access**: `http://[YOUR_IP]:8080`

The startup script will show you the exact URLs to use.

## Configuration

### Network Configuration

The application automatically detects the best network configuration:

1. **Automatic IP Detection**: The application tries to bind to `0.0.0.0:8080` (all interfaces) first, then falls back to `127.0.0.1:8080` (localhost only)
2. **Flexible Access**: The web interface automatically adapts to the server's actual IP address
3. **Manual Configuration**: To force a specific IP, edit `src/main.cpp` and modify the host parameter

**Common Network Scenarios:**
- **Local Development**: Use `127.0.0.1:8080` (localhost only)
- **Embedded Device**: Use `0.0.0.0:8080` (accessible from network)
- **Custom IP**: Modify the code to use your specific IP address

### Stream Configuration

Each stream is configured with:
- **Resolution**: 1920x1080
- **Frame Rate**: 30 FPS
- **Codec**: H.264
- **Bitrate**: 2 Mbps
- **Port Range**: 8081-8088 (one port per stream)

## API Reference

### REST Endpoints

#### Get All Streams
```http
GET /api/streams
```

Response:
```json
{
  "streams": [
    {"id": 0, "active": true},
    {"id": 1, "active": false},
    ...
  ]
}
```

#### Start Stream
```http
POST /api/stream/{id}/start
```

#### Stop Stream
```http
POST /api/stream/{id}/stop
```

#### Get Stream Status
```http
GET /api/stream/{id}/status
```

### WebSocket Events

The application provides real-time updates via WebSocket:

```javascript
// Connect to WebSocket
const ws = new WebSocket('ws://192.168.1.1:8080');

// Listen for stream updates
ws.onmessage = (event) => {
    const data = JSON.parse(event.data);
    if (data.type === 'stream_update') {
        console.log(`Stream ${data.streamId} is now ${data.active ? 'active' : 'inactive'}`);
    }
};
```

## Architecture

### Components

1. **HTTP Server** (`HttpServer.cpp`): Handles web requests and serves static files
2. **Stream Manager** (`StreamManager.cpp`): Manages multiple video streams
3. **GStreamer Pipeline** (`GStreamerPipeline.cpp`): Individual stream processing
4. **WebSocket Handler** (`WebSocketHandler.cpp`): Real-time communication
5. **Web Frontend**: Modern responsive UI built with HTML5, CSS3, and JavaScript

### Stream Flow

```
GStreamer Test Source → Video Convert → H.264 Encoder → RTP Payloader → UDP Sink
```

Each stream runs on a separate UDP port (8081-8088) and can be accessed via:
```
udp://127.0.0.1:8081  # Stream 0
udp://127.0.0.1:8082  # Stream 1
...
udp://127.0.0.1:8088  # Stream 7
```

## Development

### Building from Source

```bash
# Clone or download the source code
# Install dependencies (see above)
./install_dependencies.sh

# Build the application
./build.sh

# Run tests (if available)
cd build
make test
```

### Project Structure

```
VMS/
├── src/                    # C++ source files
│   ├── main.cpp           # Application entry point
│   ├── HttpServer.cpp     # HTTP server implementation
│   ├── StreamManager.cpp  # Stream management
│   ├── GStreamerPipeline.cpp # GStreamer integration
│   └── WebSocketHandler.cpp # WebSocket support
├── web/                   # Web frontend
│   ├── index.html         # Main web interface
│   ├── styles.css         # Styling
│   └── script.js          # JavaScript functionality
├── CMakeLists.txt         # Build configuration
├── build.sh              # Build script
├── install_dependencies.sh # Dependency installer
└── README.md             # This file
```

## Troubleshooting

### Common Issues

1. **Port 8080 already in use**
   ```bash
   # Find process using port 8080
   sudo netstat -tulpn | grep :8080
   
   # Kill the process or change the port in main.cpp
   ```

2. **GStreamer plugins not found**
   ```bash
   # Install additional GStreamer plugins
   sudo apt-get install gstreamer1.0-plugins-*
   ```

3. **Permission denied for network binding**
   ```bash
   # Run with sudo or configure proper permissions
   sudo ./vms
   ```

4. **WebSocket connection failed**
   - Check firewall settings
   - Verify the server is running
   - Check browser console for errors

### Logs

The application outputs logs to stdout. For production deployment, redirect to a log file:

```bash
sudo ./vms > /var/log/vms.log 2>&1
```

## Performance Optimization

### For Embedded Systems

1. **CPU Optimization**:
   - Use hardware-accelerated encoding if available
   - Adjust encoder settings in `GStreamerPipeline.cpp`

2. **Memory Optimization**:
   - Reduce buffer sizes in GStreamer pipeline
   - Limit concurrent streams if memory is constrained

3. **Network Optimization**:
   - Adjust bitrate based on network capacity
   - Use multicast for multiple clients

## Security Considerations

- The application runs with network privileges
- Consider implementing authentication for production use
- Use HTTPS in production environments
- Implement proper input validation for API endpoints

## License

This project is provided as-is for educational and development purposes.

## Support

For issues and questions:
1. Check the troubleshooting section
2. Review the logs for error messages
3. Verify all dependencies are properly installed
4. Ensure the system meets minimum requirements

## Future Enhancements

- [ ] Hardware-accelerated encoding support
- [ ] Authentication and user management
- [ ] Recording and playback functionality
- [ ] Mobile-responsive improvements
- [ ] Configuration file support
- [ ] System monitoring and health checks
