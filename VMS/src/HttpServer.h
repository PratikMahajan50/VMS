#pragma once

#include <string>
#include <memory>
#include <thread>
#include <atomic>
#include <map>
#include <functional>

class StreamManager;
class WebSocketHandler;

class HttpServer {
public:
    HttpServer(const std::string& host, int port, StreamManager* streamManager);
    ~HttpServer();
    
    void start();
    void stop();
    
private:
    std::string m_host;
    int m_port;
    StreamManager* m_streamManager;
    std::unique_ptr<WebSocketHandler> m_webSocketHandler;
    std::atomic<bool> m_running;
    std::thread m_serverThread;
    int m_serverSocket{-1};
    
    void serverLoop();
    std::string handleRequest(const std::string& request);
    std::string serveStaticFile(const std::string& path);
    std::string getMimeType(const std::string& path);
    std::string createApiResponse(const std::string& data);
    std::string createErrorResponse(int code, const std::string& message);
    
    // API endpoints
    std::string handleApiStreams();
    std::string handleApiStreamStart(const std::string& streamId);
    std::string handleApiStreamStop(const std::string& streamId);
    std::string handleApiStreamStatus(const std::string& streamId);
    
    // Video stream endpoints
    std::string handleVideoStream(const std::string& streamId);
};
