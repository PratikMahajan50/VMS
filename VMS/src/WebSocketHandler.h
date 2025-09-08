#pragma once

#include <string>
#include <map>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>

class StreamManager;

class WebSocketHandler {
public:
    WebSocketHandler(StreamManager* streamManager);
    ~WebSocketHandler();
    
    std::string handleWebSocketUpgrade(const std::string& request);
    void broadcastStreamUpdate(int streamId, bool active);
    
private:
    StreamManager* m_streamManager;
    std::map<int, int> m_connections;  // client socket -> stream id
    std::mutex m_connectionsMutex;
    
    std::string generateWebSocketKey(const std::string& request);
    std::string createWebSocketAccept(const std::string& key);
    std::string base64Encode(const std::string& input);
    void handleWebSocketMessage(int clientSocket, const std::string& message);
    void sendWebSocketMessage(int clientSocket, const std::string& message);
    void closeConnection(int clientSocket);
};

