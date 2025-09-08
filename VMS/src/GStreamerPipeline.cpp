#include "GStreamerPipeline.h"
#include <iostream>
#include <sstream>

GStreamerPipeline::GStreamerPipeline(int streamId, int port, int width, int height, int framerate)
    : m_streamId(streamId), m_port(port), m_width(width), m_height(height), m_framerate(framerate),
      m_pipeline(nullptr), m_source(nullptr), m_videoconvert(nullptr), m_encoder(nullptr),
      m_payloader(nullptr), m_udpsink(nullptr), m_running(false) {
}

GStreamerPipeline::~GStreamerPipeline() {
    stop();
}

bool GStreamerPipeline::initialize() {
    
    // Create uniquely named pipeline
    std::ostringstream pipelineName;
    pipelineName << "video-pipeline-" << m_streamId;
    m_pipeline = gst_pipeline_new(pipelineName.str().c_str());
    if (!m_pipeline) {
        std::cerr << "Failed to create pipeline for stream " << m_streamId << std::endl;
        return false;
    }
    
    // Create elements with unique names per stream
    std::string name;
    name = std::string("source-") + std::to_string(m_streamId);
    m_source = gst_element_factory_make("videotestsrc", name.c_str());
    name = std::string("videoconvert-") + std::to_string(m_streamId);
    m_videoconvert = gst_element_factory_make("videoconvert", name.c_str());
    name = std::string("encoder-") + std::to_string(m_streamId);
    m_encoder = gst_element_factory_make("x264enc", name.c_str());
    name = std::string("payloader-") + std::to_string(m_streamId);
    m_payloader = gst_element_factory_make("rtph264pay", name.c_str());
    name = std::string("udpsink-") + std::to_string(m_streamId);
    m_udpsink = gst_element_factory_make("udpsink", name.c_str());
    
    if (!m_source || !m_videoconvert || !m_encoder || !m_payloader || !m_udpsink) {
        std::cerr << "Failed to create GStreamer elements for stream " << m_streamId << std::endl;
        return false;
    }
    
    // Configure source
    g_object_set(m_source,
                 "pattern", 0,
                 "is-live", TRUE,
                 NULL);
    
    // Configure encoder (more deterministic across multiple instances)
    g_object_set(m_encoder,
                 "bitrate", 2000,
                 "speed-preset", 1,          // ultrafast
                 "tune", 0x00000004,         // zerolatency bitflag explicitly
                 "byte-stream", TRUE,
                 "key-int-max", 30,
                 "threads", 1,
                 NULL);
    
    // Configure payloader
    g_object_set(m_payloader,
                 "pt", 96,
                 "config-interval", 1,
                 NULL);
    
    // Configure UDP sink to server IP
    g_object_set(m_udpsink,
                 "host", "172.30.41.111",
                 "port", m_port,
                 "sync", FALSE,
                 NULL);
    
    // Add elements to pipeline
    gst_bin_add_many(GST_BIN(m_pipeline), m_source, m_videoconvert, m_encoder, m_payloader, m_udpsink, NULL);
    
    // Link elements
    if (!gst_element_link_many(m_source, m_videoconvert, m_encoder, m_payloader, m_udpsink, NULL)) {
        std::cerr << "Failed to link GStreamer elements for stream " << m_streamId << std::endl;
        return false;
    }
    
    // Start bus watch thread to log errors/states
    m_running = true;
    m_busThread = std::thread(&GStreamerPipeline::busWatch, this);
    
    // Start pipeline
    GstStateChangeReturn ret = gst_element_set_state(m_pipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        std::cerr << "Failed to start pipeline for stream " << m_streamId << std::endl;
        
        // Get more detailed error information
        GstMessage* msg = gst_bus_timed_pop_filtered(gst_pipeline_get_bus(GST_PIPELINE(m_pipeline)), 
                                                     1 * GST_SECOND, GST_MESSAGE_ERROR);
        if (msg) {
            GError* err;
            gchar* debug_info;
            gst_message_parse_error(msg, &err, &debug_info);
            std::cerr << "GStreamer error: " << err->message << std::endl;
            if (debug_info) {
                std::cerr << "Debug info: " << debug_info << std::endl;
                g_free(debug_info);
            }
            g_clear_error(&err);
            gst_message_unref(msg);
        }
        return false;
    }
    
    // Wait for pipeline to reach playing state
    ret = gst_element_get_state(m_pipeline, NULL, NULL, 5 * GST_SECOND);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        std::cerr << "Pipeline failed to reach playing state for stream " << m_streamId << std::endl;
        return false;
    }
    
    std::cout << "GStreamer pipeline started for stream " << m_streamId << " on port " << m_port << std::endl;
    
    return true;
}

void GStreamerPipeline::stop() {
    if (m_pipeline) {
        m_running = false;
        
        // Stop pipeline
        gst_element_set_state(m_pipeline, GST_STATE_NULL);

        if (m_busThread.joinable()) {
            m_busThread.join();
        }
        
        // Clean up
        gst_object_unref(m_pipeline);
        m_pipeline = nullptr;
        
        std::cout << "GStreamer pipeline stopped for stream " << m_streamId << std::endl;
    }
}

std::string GStreamerPipeline::getStreamUrl() {
    std::ostringstream url;
    url << "udp://172.30.41.111:" << m_port;
    return url.str();
}

void GStreamerPipeline::busWatch() {
    GstBus* bus = gst_pipeline_get_bus(GST_PIPELINE(m_pipeline));
    
    while (m_running) {
        GstMessage* message = gst_bus_timed_pop_filtered(bus, 100 * GST_MSECOND,
            (GstMessageType)(GST_MESSAGE_ERROR | GST_MESSAGE_EOS | GST_MESSAGE_STATE_CHANGED));
        
        if (message) {
            busCallback(bus, message, this);
            gst_message_unref(message);
        }
    }
    
    gst_object_unref(bus);
}

gboolean GStreamerPipeline::busCallback(GstBus* bus, GstMessage* message, gpointer data) {
    GStreamerPipeline* pipeline = static_cast<GStreamerPipeline*>(data);
    
    switch (GST_MESSAGE_TYPE(message)) {
        case GST_MESSAGE_ERROR: {
            GError* err;
            gchar* debug_info;
            gst_message_parse_error(message, &err, &debug_info);
            std::cerr << "GStreamer error in stream " << pipeline->m_streamId 
                      << ": " << err->message << std::endl;
            g_clear_error(&err);
            g_free(debug_info);
            break;
        }
        case GST_MESSAGE_EOS:
            std::cout << "End of stream for stream " << pipeline->m_streamId << std::endl;
            break;
        case GST_MESSAGE_STATE_CHANGED: {
            GstState old_state, new_state, pending_state;
            gst_message_parse_state_changed(message, &old_state, &new_state, &pending_state);
            if (GST_MESSAGE_SRC(message) == GST_OBJECT(pipeline->m_pipeline)) {
                std::cout << "Stream " << pipeline->m_streamId 
                          << " state changed from " << gst_element_state_get_name(old_state)
                          << " to " << gst_element_state_get_name(new_state) << std::endl;
            }
            break;
        }
        default:
            break;
    }
    
    return TRUE;
}
