#pragma once

#include <thread>
#include <atomic>
#include <chrono>

class PassiveStreamMonitor {
public:
    PassiveStreamMonitor(int streamId, int port);
    ~PassiveStreamMonitor();

    bool start();
    void stop();

    // Active if we have received packets within the last 2 seconds
    bool isActive() const;
    int port() const { return m_port; }

private:
    void receiveLoop();

    int m_streamId;
    int m_port;
    int m_socketFd;
    std::thread m_thread;
    std::atomic<bool> m_running{false};
    std::atomic<std::chrono::steady_clock::time_point> m_lastActivity;
};


