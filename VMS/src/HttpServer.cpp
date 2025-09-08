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
        std::regex streamRegex("/stream/(\\d+)");
        std::smatch matches;
        if (std::regex_match(path, matches, streamRegex)) {
            std::string streamId = matches[1].str();
            return handleVideoStream(streamId);
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
    
    // Create a simple MJPEG stream using GStreamer
    // For now, return a placeholder response
    std::ostringstream response;
    response << "HTTP/1.1 200 OK\r\n";
    response << "Content-Type: text/html\r\n";
    response << "Connection: close\r\n";
    response << "\r\n";
    response << "<!DOCTYPE html>\n";
    response << "<html><head><title>Stream " << id << "</title></head>\n";
    response << "<body style='margin:0; background:#000;'>\n";
    response << "<div style='display:flex; justify-content:center; align-items:center; height:100vh; color:#fff; font-family:Arial;'>\n";
    response << "<div style='text-align:center;'>\n";
    response << "<h1>Stream " << (id + 1) << "</h1>\n";
    response << "<p>Stream is active on UDP port " << (8081 + id) << "</p>\n";
    response << "<p>Resolution: 1920x1080 @ 30fps</p>\n";
    response << "<p>Codec: H.264</p>\n";
    response << "<div style='width:640px; height:360px; background:linear-gradient(45deg, #ff0000, #00ff00, #0000ff); margin:20px auto; border-radius:10px; display:flex; align-items:center; justify-content:center; color:#fff; font-size:24px;'>\n";
    response << "GStreamer Test Pattern<br>Stream " << (id + 1) << "\n";
    response << "</div>\n";
    response << "<p><a href='/stream/" << id << "' style='color:#fff;'>Refresh Stream</a> | <a href='/' style='color:#fff;'>Back to Dashboard</a></p>\n";
    response << "</div>\n";
    response << "</div>\n";
    response << "</body></html>\n";
    
    return response.str();
}
