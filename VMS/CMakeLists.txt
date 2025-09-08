cmake_minimum_required(VERSION 3.16)
project(VideoManagementSystem)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add compile definitions for C++17 features
add_compile_definitions(_GLIBCXX_USE_CXX11_ABI=1)

# Find required packages
find_package(PkgConfig REQUIRED)
pkg_check_modules(GSTREAMER REQUIRED gstreamer-1.0)
pkg_check_modules(GSTREAMER_APP REQUIRED gstreamer-app-1.0)
pkg_check_modules(GSTREAMER_VIDEO REQUIRED gstreamer-video-1.0)

# Find OpenSSL for HTTPS support
find_package(OpenSSL REQUIRED)

# Include directories
include_directories(${GSTREAMER_INCLUDE_DIRS})
include_directories(${GSTREAMER_APP_INCLUDE_DIRS})
include_directories(${GSTREAMER_VIDEO_INCLUDE_DIRS})

# Add compile flags
add_compile_options(${GSTREAMER_CFLAGS_OTHER})
add_compile_options(${GSTREAMER_APP_CFLAGS_OTHER})
add_compile_options(${GSTREAMER_VIDEO_CFLAGS_OTHER})

# Source files
set(SOURCES
    src/main.cpp
    src/HttpServer.cpp
    src/StreamManager.cpp
    src/GStreamerPipeline.cpp
    src/WebSocketHandler.cpp
    src/PassiveStreamMonitor.cpp
)

# Create executable
add_executable(vms ${SOURCES})

# Link libraries
target_link_libraries(vms 
    ${GSTREAMER_LIBRARIES}
    ${GSTREAMER_APP_LIBRARIES}
    ${GSTREAMER_VIDEO_LIBRARIES}
    OpenSSL::SSL
    OpenSSL::Crypto
    pthread
    m
    dl
)

# Copy web assets to build directory
file(COPY web DESTINATION ${CMAKE_BINARY_DIR})

# Installation
install(TARGETS vms DESTINATION bin)
install(DIRECTORY web DESTINATION share/vms)
