#include "StreamManager.h"
#include <iostream>
#include <sstream>
#include <thread>
#include <chrono>

StreamManager::StreamManager() : m_nextPort(8081) {
    std::cout << "StreamManager initialized" << std::endl;
}

StreamManager::~StreamManager() {
    stopAllStreams();
}

bool StreamManager::startStream(int streamId, int width, int height, int framerate) {
    std::lock_guard<std::mutex> lock(m_streamsMutex);
    
    // External source mode: only create a passive monitor and mark port
    if (m_streams.find(streamId) != m_streams.end()) {
        std::cout << "Stream " << streamId << " already monitored" << std::endl;
        return true;
    }
    
    int port = getNextAvailablePort();
    auto monitor = std::make_unique<PassiveStreamMonitor>(streamId, port);
    if (!monitor->start()) {
        std::cerr << "Failed to start monitor for stream " << streamId << std::endl;
        return false;
    }
    m_monitors[streamId] = std::move(monitor);
    std::cout << "Monitoring external stream " << streamId << " on UDP port " << port << std::endl;
    return true;
}

bool StreamManager::stopStream(int streamId) {
    std::lock_guard<std::mutex> lock(m_streamsMutex);
    
    auto itM = m_monitors.find(streamId);
    if (itM != m_monitors.end()) {
        itM->second->stop();
        m_monitors.erase(itM);
        std::cout << "Stopped monitoring stream " << streamId << std::endl;
        return true;
    }
    std::cout << "Monitor for stream " << streamId << " not found" << std::endl;
    return false;
}

bool StreamManager::isStreamActive(int streamId) {
    std::lock_guard<std::mutex> lock(m_streamsMutex);
    auto itM = m_monitors.find(streamId);
    if (itM != m_monitors.end()) {
        return itM->second->isActive();
    }
    return false;
}

void StreamManager::stopAllStreams() {
    std::lock_guard<std::mutex> lock(m_streamsMutex);
    for (auto& m : m_monitors) {
        m.second->stop();
        std::cout << "Stopped monitoring stream " << m.first << std::endl;
    }
    m_monitors.clear();
    std::cout << "All monitors stopped" << std::endl;
}

std::map<int, bool> StreamManager::getStreamStatus() {
    std::lock_guard<std::mutex> lock(m_streamsMutex);
    std::map<int, bool> status;
    for (const auto& m : m_monitors) {
        status[m.first] = m.second->isActive();
    }
    return status;
}

std::string StreamManager::getStreamUrl(int streamId) {
    std::lock_guard<std::mutex> lock(m_streamsMutex);
    auto itM = m_monitors.find(streamId);
    if (itM != m_monitors.end()) {
        std::ostringstream url; url << "udp://172.30.41.111:" << (8081 + streamId); return url.str();
    }
    return "";
}

int StreamManager::getNextAvailablePort() {
    return m_nextPort++;
}
