#pragma once

#include <gst/gst.h>
#include <gst/app/gstappsrc.h>
#include <string>
#include <atomic>
#include <thread>

class GStreamerPipeline {
public:
    GStreamerPipeline(int streamId, int port, int width, int height, int framerate);
    ~GStreamerPipeline();
    
    bool initialize();
    void stop();
    std::string getStreamUrl();
    void setTestPattern(int pattern);
    
private:
    int m_streamId;
    int m_port;
    int m_width;
    int m_height;
    int m_framerate;
    
    GstElement* m_pipeline;
    GstElement* m_source;
    GstElement* m_videoconvert;
    GstElement* m_encoder;
    GstElement* m_payloader;
    GstElement* m_udpsink;
    
    std::atomic<bool> m_running;
    std::thread m_busThread;
    
    void busWatch();
    static gboolean busCallback(GstBus* bus, GstMessage* message, gpointer data);
    std::string createPipelineString();
};

