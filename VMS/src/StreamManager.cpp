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

    // If already running, nothing to do
    if (m_streams.find(streamId) != m_streams.end()) {
        std::cout << "Stream " << streamId << " already running" << std::endl;
        return true;
    }

    // Allocate a new UDP port and create a pipeline
    int port = getNextAvailablePort();
    auto pipeline = std::make_unique<GStreamerPipeline>(streamId, port, width, height, framerate);
    if (!pipeline->initialize()) {
        std::cerr << "Failed to start GStreamer pipeline for stream " << streamId << std::endl;
        return false;
    }
    m_streams[streamId] = std::move(pipeline);
    std::cout << "Started GStreamer pipeline for stream " << streamId << " on UDP port " << port << std::endl;
    return true;
}

bool StreamManager::stopStream(int streamId) {
    std::lock_guard<std::mutex> lock(m_streamsMutex);

    auto it = m_streams.find(streamId);
    if (it != m_streams.end()) {
        it->second->stop();
        m_streams.erase(it);
        std::cout << "Stopped stream " << streamId << std::endl;
        return true;
    }
    std::cout << "Stream " << streamId << " not found" << std::endl;
    return false;
}

bool StreamManager::isStreamActive(int streamId) {
    std::lock_guard<std::mutex> lock(m_streamsMutex);
    return m_streams.find(streamId) != m_streams.end();
}

void StreamManager::stopAllStreams() {
    std::lock_guard<std::mutex> lock(m_streamsMutex);
    for (auto& s : m_streams) {
        s.second->stop();
        std::cout << "Stopped stream " << s.first << std::endl;
    }
    m_streams.clear();
    std::cout << "All streams stopped" << std::endl;
}

std::map<int, bool> StreamManager::getStreamStatus() {
    std::lock_guard<std::mutex> lock(m_streamsMutex);
    std::map<int, bool> status;
    for (const auto& s : m_streams) {
        status[s.first] = true;
    }
    return status;
}

std::string StreamManager::getStreamUrl(int streamId) {
    std::lock_guard<std::mutex> lock(m_streamsMutex);
    auto it = m_streams.find(streamId);
    if (it != m_streams.end()) {
        return it->second->getStreamUrl();
    }
    return "";
}

int StreamManager::getNextAvailablePort() {
    return m_nextPort++;
}
