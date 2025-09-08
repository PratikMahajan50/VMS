#include "WebSocketHandler.h"
#include "StreamManager.h"
#include <iostream>
#include <sstream>
#include <regex>
#include <openssl/sha.h>
#include <openssl/evp.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <unistd.h>
#include <sys/socket.h>

WebSocketHandler::WebSocketHandler(StreamManager* streamManager)
    : m_streamManager(streamManager) {
}

WebSocketHandler::~WebSocketHandler() {
    std::lock_guard<std::mutex> lock(m_connectionsMutex);
    for (auto& pair : m_connections) {
        close(pair.first);
    }
    m_connections.clear();
}

std::string WebSocketHandler::handleWebSocketUpgrade(const std::string& request) {
    // Extract WebSocket key
    std::regex keyRegex("Sec-WebSocket-Key: ([A-Za-z0-9+/=]+)");
    std::smatch matches;
    
    if (!std::regex_search(request, matches, keyRegex)) {
        return "HTTP/1.1 400 Bad Request\r\n\r\n";
    }
    
    std::string key = matches[1].str();
    std::string accept = createWebSocketAccept(key);
    
    // Create WebSocket response
    std::ostringstream response;
    response << "HTTP/1.1 101 Switching Protocols\r\n";
    response << "Upgrade: websocket\r\n";
    response << "Connection: Upgrade\r\n";
    response << "Sec-WebSocket-Accept: " << accept << "\r\n";
    response << "\r\n";
    
    return response.str();
}

void WebSocketHandler::broadcastStreamUpdate(int streamId, bool active) {
    std::lock_guard<std::mutex> lock(m_connectionsMutex);
    
    std::ostringstream message;
    message << "{\"type\":\"stream_update\",\"streamId\":" << streamId 
            << ",\"active\":" << (active ? "true" : "false") << "}";
    
    for (auto& pair : m_connections) {
        sendWebSocketMessage(pair.first, message.str());
    }
}

std::string WebSocketHandler::generateWebSocketKey(const std::string& request) {
    std::regex keyRegex("Sec-WebSocket-Key: ([A-Za-z0-9+/=]+)");
    std::smatch matches;
    
    if (std::regex_search(request, matches, keyRegex)) {
        return matches[1].str();
    }
    
    return "";
}

std::string WebSocketHandler::createWebSocketAccept(const std::string& key) {
    const std::string magic = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    std::string combined = key + magic;
    
    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1(reinterpret_cast<const unsigned char*>(combined.c_str()), combined.length(), hash);
    
    return base64Encode(std::string(reinterpret_cast<char*>(hash), SHA_DIGEST_LENGTH));
}

std::string WebSocketHandler::base64Encode(const std::string& input) {
    BIO* bio = BIO_new(BIO_s_mem());
    BIO* b64 = BIO_new(BIO_f_base64());
    bio = BIO_push(b64, bio);
    
    BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);
    BIO_write(bio, input.c_str(), input.length());
    BIO_flush(bio);
    
    BUF_MEM* bufferPtr;
    BIO_get_mem_ptr(bio, &bufferPtr);
    
    std::string result(bufferPtr->data, bufferPtr->length);
    BIO_free_all(bio);
    
    return result;
}

void WebSocketHandler::handleWebSocketMessage(int clientSocket, const std::string& message) {
    // Simple JSON message handling
    if (message.find("\"type\":\"subscribe\"") != std::string::npos) {
        std::regex streamRegex("\"streamId\":(\\d+)");
        std::smatch matches;
        if (std::regex_search(message, matches, streamRegex)) {
            int streamId = std::stoi(matches[1].str());
            std::lock_guard<std::mutex> lock(m_connectionsMutex);
            m_connections[clientSocket] = streamId;
        }
    }
}

void WebSocketHandler::sendWebSocketMessage(int clientSocket, const std::string& message) {
    // Simple WebSocket frame (no masking for server)
    std::ostringstream frame;
    
    // FIN + opcode (text frame)
    frame << static_cast<char>(0x81);
    
    // Payload length
    if (message.length() < 126) {
        frame << static_cast<char>(message.length());
    } else if (message.length() < 65536) {
        frame << static_cast<char>(126);
        frame << static_cast<char>((message.length() >> 8) & 0xFF);
        frame << static_cast<char>(message.length() & 0xFF);
    }
    
    // Payload
    frame << message;
    
    send(clientSocket, frame.str().c_str(), frame.str().length(), 0);
}

void WebSocketHandler::closeConnection(int clientSocket) {
    std::lock_guard<std::mutex> lock(m_connectionsMutex);
    auto it = m_connections.find(clientSocket);
    if (it != m_connections.end()) {
        m_connections.erase(it);
    }
    close(clientSocket);
}
