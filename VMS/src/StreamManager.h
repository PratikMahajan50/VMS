#pragma once

#include <map>
#include <memory>
#include <mutex>
#include <atomic>
#include "GStreamerPipeline.h"
#include "PassiveStreamMonitor.h"

class StreamManager {
public:
    StreamManager();
    ~StreamManager();
    
    bool startStream(int streamId, int width, int height, int framerate);
    bool stopStream(int streamId);
    bool isStreamActive(int streamId);
    void stopAllStreams();
    
    std::map<int, bool> getStreamStatus();
    std::string getStreamUrl(int streamId);
    
private:
    std::map<int, std::unique_ptr<GStreamerPipeline>> m_streams;
    std::map<int, std::unique_ptr<PassiveStreamMonitor>> m_monitors;
    std::mutex m_streamsMutex;
    std::atomic<int> m_nextPort;
    
    int getNextAvailablePort();
};

