#include "PassiveStreamMonitor.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <iostream>

PassiveStreamMonitor::PassiveStreamMonitor(int streamId, int port)
    : m_streamId(streamId), m_port(port), m_socketFd(-1), 
      m_lastActivity(std::chrono::steady_clock::now()) {
}

PassiveStreamMonitor::~PassiveStreamMonitor() {
    stop();
}

bool PassiveStreamMonitor::start() {
    if (m_running.load()) return true;

    m_socketFd = socket(AF_INET, SOCK_DGRAM, 0);
    if (m_socketFd < 0) {
        std::cerr << "Monitor: failed to create UDP socket for stream " << m_streamId << std::endl;
        return false;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(m_port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    int reuse = 1;
    setsockopt(m_socketFd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    if (bind(m_socketFd, (sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr << "Monitor: failed to bind UDP port " << m_port << " for stream " << m_streamId << std::endl;
        close(m_socketFd);
        m_socketFd = -1;
        return false;
    }

    m_running = true;
    m_thread = std::thread(&PassiveStreamMonitor::receiveLoop, this);
    return true;
}

void PassiveStreamMonitor::stop() {
    if (!m_running.exchange(false)) return;
    if (m_socketFd >= 0) {
        close(m_socketFd);
        m_socketFd = -1;
    }
    if (m_thread.joinable()) m_thread.join();
}

bool PassiveStreamMonitor::isActive() const {
    auto last = m_lastActivity.load();
    auto now = std::chrono::steady_clock::now();
    return (now - last) < std::chrono::seconds(2);
}

void PassiveStreamMonitor::receiveLoop() {
    constexpr size_t BUF_SIZE = 1500;
    char buffer[BUF_SIZE];
    while (m_running.load()) {
        ssize_t n = recv(m_socketFd, buffer, sizeof(buffer), 0);
        if (n > 0) {
            m_lastActivity.store(std::chrono::steady_clock::now());
        } else if (n < 0) {
            if (!m_running.load()) break;
            // avoid busy loop
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }
}


