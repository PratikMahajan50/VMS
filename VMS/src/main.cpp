#include <iostream>
#include <signal.h>
#include <memory>
#include <thread>
#include <chrono>
#include "HttpServer.h"
#include "StreamManager.h"
#include <gst/gst.h>

std::unique_ptr<HttpServer> g_server;
std::unique_ptr<StreamManager> g_streamManager;
static volatile sig_atomic_t g_shutdownRequested = 0;

void signalHandler(int /*signum*/) {
    g_shutdownRequested = 1;
}

int main() {
    // Set up signal handlers
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    std::cout << "Starting Video Management System..." << std::endl;
    
    try {
        // Initialize GStreamer once
        gst_init(nullptr, nullptr);
        
        // Initialize stream manager
        g_streamManager = std::make_unique<StreamManager>();
        
        // Initialize HTTP server for Ubuntu deployment
        std::string host = "172.30.41.111";  // Bind to device IP
        int port = 8080;
        
        g_server = std::make_unique<HttpServer>(host, port, g_streamManager.get());
        
        // Start HTTP server
        std::cout << "Starting HTTP server on " << host << ":" << port << "..." << std::endl;
        std::cout << "Web interface: http://" << host << ":" << port << std::endl;
        
        // Start server in background thread
        g_server->start();
        
        // Start all streams after server is up
        std::cout << "Starting 8 video streams..." << std::endl;
        for (int i = 0; i < 8; ++i) {
            std::cout << "Starting stream " << i << "..." << std::endl;
            bool success = g_streamManager->startStream(i, 1920, 1080, 30);
            if (success) {
                std::cout << "Stream " << i << " started successfully" << std::endl;
            } else {
                std::cerr << "Failed to start stream " << i << std::endl;
            }
        }

        // Keep main thread alive and react to Ctrl+C
        std::cout << "VMS is running. Press Ctrl+C to stop." << std::endl;
        while (!g_shutdownRequested) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
        std::cout << "Received shutdown request. Stopping services..." << std::endl;
        if (g_server) g_server->stop();
        if (g_streamManager) g_streamManager->stopAllStreams();
        std::cout << "Shutdown complete." << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
