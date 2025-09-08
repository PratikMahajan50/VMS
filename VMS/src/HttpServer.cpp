#include "HttpServer.h"
#include "StreamManager.h"
#include "WebSocketHandler.h"
#include <iostream>
#include <sstream>
#include <fstream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <regex>
#include <errno.h>
#include <fcntl.h>
#include <chrono>
#include <thread>
#include <cstring>
#include <sys/select.h>

HttpServer::HttpServer(const std::string& host, int port, StreamManager* streamManager)
    : m_host(host), m_port(port), m_streamManager(streamManager), m_running(false) {
    m_webSocketHandler = std::make_unique<WebSocketHandler>(streamManager);
}

HttpServer::~HttpServer() {
    stop();
}

void HttpServer::start() {
    m_running = true;
    m_serverThread = std::thread(&HttpServer::serverLoop, this);
}

void HttpServer::stop() {
    m_running = false;
    // Close the listening socket to unblock accept()
    if (m_serverSocket >= 0) {
        close(m_serverSocket);
        m_serverSocket = -1;
    }
    if (m_serverThread.joinable()) {
        m_serverThread.join();
    }
}

void HttpServer::serverLoop() {
    m_serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (m_serverSocket < 0) {
        throw std::runtime_error("Failed to create socket");
    }
    
    // Set socket options
    int opt = 1;
    setsockopt(m_serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(m_port);
    inet_pton(AF_INET, m_host.c_str(), &serverAddr.sin_addr);
    
    if (bind(m_serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        close(m_serverSocket);
        std::string errorMsg = "Failed to bind socket to " + m_host + ":" + std::to_string(m_port);
        if (errno == EADDRINUSE) {
            errorMsg += " (Port already in use)";
        } else if (errno == EADDRNOTAVAIL) {
            errorMsg += " (Address not available)";
        } else if (errno == EACCES) {
            errorMsg += " (Permission denied - try running with sudo)";
        } else {
            errorMsg += " (Error: " + std::to_string(errno) + ")";
        }
        throw std::runtime_error(errorMsg);
    }
    
    if (listen(m_serverSocket, 10) < 0) {
        close(m_serverSocket);
        throw std::runtime_error("Failed to listen on socket");
    }
    
    std::cout << "HTTP server listening on " << m_host << ":" << m_port << std::endl;
    std::cout << "Server is running... Press Ctrl+C to stop" << std::endl;
    
    while (m_running) {
        // Wait for readiness with timeout so we can react to stop()
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(m_serverSocket, &readfds);
        timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 200000; // 200ms

        int sel = select(m_serverSocket + 1, &readfds, nullptr, nullptr, &tv);
        if (!m_running) break;
        if (sel <= 0) {
            // timeout or interrupted
            continue;
        }

        sockaddr_in clientAddr;
        socklen_t clientLen = sizeof(clientAddr);
        int clientSocket = accept(m_serverSocket, (sockaddr*)&clientAddr, &clientLen);
        if (clientSocket < 0) {
            if (!m_running) break;
            // If socket was closed during stop(), exit loop
            if (errno == EBADF || errno == EINVAL) break;
            continue;
        }

        // Handle request in a separate thread
        std::thread([this, clientSocket]() {
            char buffer[4096];
            int bytesRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
            if (bytesRead > 0) {
                buffer[bytesRead] = '\0';
                std::string request(buffer);
                std::string response = handleRequest(request);
                send(clientSocket, response.c_str(), response.length(), 0);
            }
            close(clientSocket);
        }).detach();
    }
    
    if (m_serverSocket >= 0) {
        close(m_serverSocket);
        m_serverSocket = -1;
    }
}

std::string HttpServer::handleRequest(const std::string& request) {
    std::istringstream requestStream(request);
    std::string method, path, version;
    requestStream >> method >> path >> version;
    
    // Handle WebSocket upgrade
    if (method == "GET" && request.find("Upgrade: websocket") != std::string::npos) {
        return m_webSocketHandler->handleWebSocketUpgrade(request);
    }
    
    // Handle API endpoints
    if (path.find("/api/") == 0) {
        if (path == "/api/streams") {
            return handleApiStreams();
        } else if (path.find("/api/stream/") == 0) {
            std::regex streamRegex("/api/stream/(\\d+)/(start|stop|status)");
            std::smatch matches;
            if (std::regex_match(path, matches, streamRegex)) {
                std::string streamId = matches[1].str();
                std::string action = matches[2].str();
                
                if (action == "start") {
                    return handleApiStreamStart(streamId);
                } else if (action == "stop") {
                    return handleApiStreamStop(streamId);
                } else if (action == "status") {
                    return handleApiStreamStatus(streamId);
                }
            }
        }
        return createErrorResponse(404, "API endpoint not found");
    }
    
    // Handle video stream endpoints
    if (path.find("/stream/") == 0) {
        std::regex streamRegex("/stream/(\\d+)(?:/(\\w+))?");
        std::smatch matches;
        if (std::regex_match(path, matches, streamRegex)) {
            std::string streamId = matches[1].str();
            if (matches.size() > 2 && matches[2].str() == "mjpeg") {
                return handleMJPEGStream(streamId);
            } else {
                return handleVideoStream(streamId);
            }
        }
    }
    
    // Serve static files
    if (path == "/") {
        path = "/index.html";
    }
    
    return serveStaticFile(path);
}

std::string HttpServer::serveStaticFile(const std::string& path) {
    std::string filePath = "web" + path;
    
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        return createErrorResponse(404, "File not found");
    }
    
    std::ostringstream content;
    content << file.rdbuf();
    file.close();
    
    std::string mimeType = getMimeType(path);
    
    std::ostringstream response;
    response << "HTTP/1.1 200 OK\r\n";
    response << "Content-Type: " << mimeType << "\r\n";
    response << "Content-Length: " << content.str().length() << "\r\n";
    response << "Access-Control-Allow-Origin: *\r\n";
    response << "Connection: close\r\n";
    response << "\r\n";
    response << content.str();
    
    return response.str();
}

std::string HttpServer::getMimeType(const std::string& path) {
    if (path.length() >= 5 && path.substr(path.length() - 5) == ".html") return "text/html";
    if (path.length() >= 4 && path.substr(path.length() - 4) == ".css") return "text/css";
    if (path.length() >= 3 && path.substr(path.length() - 3) == ".js") return "application/javascript";
    if (path.length() >= 5 && path.substr(path.length() - 5) == ".json") return "application/json";
    if (path.length() >= 4 && path.substr(path.length() - 4) == ".png") return "image/png";
    if ((path.length() >= 4 && path.substr(path.length() - 4) == ".jpg") || 
        (path.length() >= 5 && path.substr(path.length() - 5) == ".jpeg")) return "image/jpeg";
    if (path.length() >= 4 && path.substr(path.length() - 4) == ".gif") return "image/gif";
    if (path.length() >= 4 && path.substr(path.length() - 4) == ".svg") return "image/svg+xml";
    return "text/plain";
}

std::string HttpServer::createApiResponse(const std::string& data) {
    std::ostringstream response;
    response << "HTTP/1.1 200 OK\r\n";
    response << "Content-Type: application/json\r\n";
    response << "Content-Length: " << data.length() << "\r\n";
    response << "Access-Control-Allow-Origin: *\r\n";
    response << "Connection: close\r\n";
    response << "\r\n";
    response << data;
    return response.str();
}

std::string HttpServer::createErrorResponse(int code, const std::string& message) {
    std::ostringstream response;
    response << "HTTP/1.1 " << code << " " << (code == 404 ? "Not Found" : "Internal Server Error") << "\r\n";
    response << "Content-Type: application/json\r\n";
    response << "Access-Control-Allow-Origin: *\r\n";
    response << "Connection: close\r\n";
    response << "\r\n";
    response << "{\"error\": \"" << message << "\"}";
    return response.str();
}

std::string HttpServer::handleApiStreams() {
    auto streams = m_streamManager->getStreamStatus();
    std::ostringstream json;
    json << "{\"streams\": [";
    
    bool first = true;
    for (const auto& stream : streams) {
        if (!first) json << ",";
        json << "{\"id\": " << stream.first 
             << ", \"active\": " << (stream.second ? "true" : "false") << "}";
        first = false;
    }
    
    json << "]}";
    return createApiResponse(json.str());
}

std::string HttpServer::handleApiStreamStart(const std::string& streamId) {
    int id = std::stoi(streamId);
    bool success = m_streamManager->startStream(id, 1920, 1080, 30);
    
    std::ostringstream json;
    json << "{\"success\": " << (success ? "true" : "false") 
         << ", \"streamId\": " << id << "}";
    
    return createApiResponse(json.str());
}

std::string HttpServer::handleApiStreamStop(const std::string& streamId) {
    int id = std::stoi(streamId);
    bool success = m_streamManager->stopStream(id);
    
    std::ostringstream json;
    json << "{\"success\": " << (success ? "true" : "false") 
         << ", \"streamId\": " << id << "}";
    
    return createApiResponse(json.str());
}

std::string HttpServer::handleApiStreamStatus(const std::string& streamId) {
    int id = std::stoi(streamId);
    bool active = m_streamManager->isStreamActive(id);
    
    std::ostringstream json;
    json << "{\"streamId\": " << id 
         << ", \"active\": " << (active ? "true" : "false") << "}";
    
    return createApiResponse(json.str());
}

std::string HttpServer::handleVideoStream(const std::string& streamId) {
    int id = std::stoi(streamId);
    
    // Check if stream is active
    if (!m_streamManager->isStreamActive(id)) {
        return createErrorResponse(404, "Stream not found or inactive");
    }
    
    // Create an HTML page with WebRTC or HLS streaming capability
    std::ostringstream response;
    response << "HTTP/1.1 200 OK\r\n";
    response << "Content-Type: text/html\r\n";
    response << "Connection: close\r\n";
    response << "\r\n";
    response << "<!DOCTYPE html>\n";
    response << "<html><head>\n";
    response << "<title>Stream " << (id + 1) << "</title>\n";
    response << "<style>\n";
    response << "body { margin:0; background:#000; color:#fff; font-family:Arial; }\n";
    response << ".container { display:flex; flex-direction:column; height:100vh; }\n";
    response << ".header { background:#333; padding:1rem; text-align:center; }\n";
    response << ".video-container { flex:1; display:flex; justify-content:center; align-items:center; position:relative; }\n";
    response << ".video-player { max-width:100%; max-height:100%; border:2px solid #555; }\n";
    response << ".controls { background:#333; padding:1rem; text-align:center; }\n";
    response << ".btn { background:#007bff; color:white; border:none; padding:0.5rem 1rem; margin:0 0.5rem; cursor:pointer; border-radius:4px; }\n";
    response << ".btn:hover { background:#0056b3; }\n";
    response << ".status { margin:1rem 0; }\n";
    response << ".error { color:#ff6b6b; }\n";
    response << ".success { color:#51cf66; }\n";
    response << "</style>\n";
    response << "</head>\n";
    response << "<body>\n";
    response << "<div class='container'>\n";
    response << "<div class='header'>\n";
    response << "<h1>Stream " << (id + 1) << " - Live View</h1>\n";
    response << "<p>UDP Port: " << (8081 + id) << " | Resolution: 1920x1080 @ 30fps | Codec: H.264</p>\n";
    response << "</div>\n";
    response << "<div class='video-container'>\n";
    response << "<canvas id='videoCanvas' class='video-player' width='640' height='360'></canvas>\n";
    response << "<div id='status' class='status'>Stream " << (id + 1) << " - GStreamer Test Pattern</div>\n";
    response << "</div>\n";
    response << "<div class='controls'>\n";
    response << "<button class='btn' onclick='refreshStream()'>Refresh Stream</button>\n";
    response << "<button class='btn' onclick='toggleFullscreen()'>Fullscreen</button>\n";
    response << "<button class='btn' onclick='window.close()'>Close</button>\n";
    response << "<a href='/' class='btn' style='text-decoration:none;'>Back to Dashboard</a>\n";
    response << "</div>\n";
    response << "</div>\n";
    response << "<script>\n";
    response << "const canvas = document.getElementById('videoCanvas');\n";
    response << "const ctx = canvas.getContext('2d');\n";
    response << "const status = document.getElementById('status');\n";
    response << "const streamId = " << id << ";\n";
    response << "let animationId;\n";
    response << "let time = 0;\n";
    response << "\n";
    response << "function updateStatus(message, type = 'info') {\n";
    response << "    status.textContent = message;\n";
    response << "    status.className = 'status ' + type;\n";
    response << "}\n";
    response << "\n";
    response << "function drawTestPattern() {\n";
    response << "    const width = canvas.width;\n";
    response << "    const height = canvas.height;\n";
    response << "    \n";
    response << "    // Clear canvas\n";
    response << "    ctx.fillStyle = '#000';\n";
    response << "    ctx.fillRect(0, 0, width, height);\n";
    response << "    \n";
    response << "    // Draw SMPTE color bars (pattern 2 from videotestsrc)\n";
    response << "    const barWidth = width / 7;\n";
    response << "    const colors = ['#C0C0C0', '#C0C000', '#00C0C0', '#00C000', '#C000C0', '#C00000', '#0000C0'];\n";
    response << "    \n";
    response << "    for (let i = 0; i < 7; i++) {\n";
    response << "        ctx.fillStyle = colors[i];\n";
    response << "        ctx.fillRect(i * barWidth, 0, barWidth, height * 0.6);\n";
    response << "    }\n";
    response << "    \n";
    response << "    // Draw moving elements\n";
    response << "    const centerX = width / 2;\n";
    response << "    const centerY = height / 2;\n";
    response << "    \n";
    response << "    // Moving circle\n";
    response << "    const circleX = centerX + Math.sin(time * 0.02) * 100;\n";
    response << "    const circleY = centerY + Math.cos(time * 0.02) * 50;\n";
    response << "    ctx.fillStyle = '#FF0000';\n";
    response << "    ctx.beginPath();\n";
    response << "    ctx.arc(circleX, circleY, 20, 0, Math.PI * 2);\n";
    response << "    ctx.fill();\n";
    response << "    \n";
    response << "    // Moving rectangle\n";
    response << "    const rectX = centerX + Math.cos(time * 0.015) * 80;\n";
    response << "    const rectY = centerY + Math.sin(time * 0.015) * 40;\n";
    response << "    ctx.fillStyle = '#00FF00';\n";
    response << "    ctx.fillRect(rectX - 15, rectY - 15, 30, 30);\n";
    response << "    \n";
    response << "    // Moving triangle\n";
    response << "    const triX = centerX + Math.sin(time * 0.025) * 60;\n";
    response << "    const triY = centerY + Math.cos(time * 0.025) * 30;\n";
    response << "    ctx.fillStyle = '#0000FF';\n";
    response << "    ctx.beginPath();\n";
    response << "    ctx.moveTo(triX, triY - 15);\n";
    response << "    ctx.lineTo(triX - 15, triY + 15);\n";
    response << "    ctx.lineTo(triX + 15, triY + 15);\n";
    response << "    ctx.closePath();\n";
    response << "    ctx.fill();\n";
    response << "    \n";
    response << "    // Draw text overlay\n";
    response << "    ctx.fillStyle = '#FFFFFF';\n";
    response << "    ctx.font = '16px Arial';\n";
    response << "    ctx.textAlign = 'center';\n";
    response << "    ctx.fillText('GStreamer Test Pattern - Stream ' + (streamId + 1), centerX, height - 20);\n";
    response << "    ctx.fillText('Time: ' + Math.floor(time / 60) + 's', centerX, height - 40);\n";
    response << "    \n";
    response << "    time++;\n";
    response << "}\n";
    response << "\n";
    response << "function animate() {\n";
    response << "    drawTestPattern();\n";
    response << "    animationId = requestAnimationFrame(animate);\n";
    response << "}\n";
    response << "\n";
    response << "function refreshStream() {\n";
    response << "    updateStatus('Refreshing stream...', 'info');\n";
    response << "    if (animationId) {\n";
    response << "        cancelAnimationFrame(animationId);\n";
    response << "    }\n";
    response << "    time = 0;\n";
    response << "    animate();\n";
    response << "    updateStatus('Stream " << (id + 1) << " - GStreamer Test Pattern (Simulated)', 'success');\n";
    response << "}\n";
    response << "\n";
    response << "function toggleFullscreen() {\n";
    response << "    if (canvas.requestFullscreen) {\n";
    response << "        canvas.requestFullscreen();\n";
    response << "    } else if (canvas.webkitRequestFullscreen) {\n";
    response << "        canvas.webkitRequestFullscreen();\n";
    response << "    } else if (canvas.msRequestFullscreen) {\n";
    response << "        canvas.msRequestFullscreen();\n";
    response << "    }\n";
    response << "}\n";
    response << "\n";
    response << "// Start the animation\n";
    response << "updateStatus('Starting stream...', 'info');\n";
    response << "setTimeout(() => {\n";
    response << "    refreshStream();\n";
    response << "}, 500);\n";
    response << "\n";
    response << "// Cleanup on page unload\n";
    response << "window.addEventListener('beforeunload', () => {\n";
    response << "    if (animationId) {\n";
    response << "        cancelAnimationFrame(animationId);\n";
    response << "    }\n";
    response << "});\n";
    response << "</script>\n";
    response << "</body></html>\n";
    
    return response.str();
}

std::string HttpServer::handleMJPEGStream(const std::string& streamId) {
    int id = std::stoi(streamId);
    
    // Check if stream is active
    if (!m_streamManager->isStreamActive(id)) {
        return createErrorResponse(404, "Stream not found or inactive");
    }
    
    // For now, return a simple test pattern as MJPEG
    // In a real implementation, this would connect to the GStreamer pipeline
    // and convert the H.264 stream to MJPEG for web viewing
    
    std::ostringstream response;
    response << "HTTP/1.1 200 OK\r\n";
    response << "Content-Type: multipart/x-mixed-replace; boundary=--myboundary\r\n";
    response << "Cache-Control: no-cache\r\n";
    response << "Connection: close\r\n";
    response << "\r\n";
    
    // Generate a simple test pattern as MJPEG frames
    // This is a placeholder - in production you'd use GStreamer to convert H.264 to MJPEG
    for (int frame = 0; frame < 10; frame++) {
        response << "--myboundary\r\n";
        response << "Content-Type: image/jpeg\r\n";
        response << "Content-Length: 0\r\n";
        response << "\r\n";
        response << "\r\n";
        
        // In a real implementation, you would:
        // 1. Capture a frame from the GStreamer pipeline
        // 2. Convert it to JPEG format
        // 3. Send the JPEG data here
        
        // For now, we'll just send a minimal response
        // The browser will show "broken image" but the connection will be established
    }
    
    return response.str();
}
