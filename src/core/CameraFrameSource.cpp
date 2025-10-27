#include "core/CameraFrameSource.h"
#include "utils/Logger.h"
#include <opencv2/imgproc.hpp>
#include <thread>
#include <chrono>
#include <cstdlib>
#include <cstring>

namespace TVLED {

CameraFrameSource::CameraFrameSource(const std::string& device, int width, int height, int fps, int sensor_mode,
                                     bool enable_scaling, int scaled_width, int scaled_height)
    : device_(device), width_(width), height_(height), fps_(fps), sensor_mode_(sensor_mode),
      initialized_(false), camera_pipe_(nullptr),
      enable_scaling_(enable_scaling), scaled_width_(scaled_width), scaled_height_(scaled_height) {
}

CameraFrameSource::~CameraFrameSource() {
    release();
}

int CameraFrameSource::parseCameraIndex() const {
    try {
        return std::stoi(device_);
    } catch (...) {
        size_t pos = device_.rfind("video");
        if (pos != std::string::npos && pos + 5 < device_.length()) {
            try {
                return std::stoi(device_.substr(pos + 5));
            } catch (...) {
                return 0;
            }
        }
    }
    return 0;
}

bool CameraFrameSource::initialize() {
    LOG_INFO("Initializing camera (simple pipe method): " + device_ + 
             " at " + std::to_string(width_) + "x" + std::to_string(height_) + 
             "@" + std::to_string(fps_) + "fps");
    
    try {
        int camera_index = parseCameraIndex();
        
        // Build rpicam-vid command
        // Output raw RGB24 frames to stdout for maximum simplicity
        std::string cmd = "rpicam-vid";
        cmd += " --camera " + std::to_string(camera_index);
        
        // Add sensor mode if specified (forces specific sensor mode to avoid cropping)
        if (sensor_mode_ >= 0) {
            cmd += " --mode " + std::to_string(width_) + ":" + std::to_string(height_);
        }
        
        cmd += " --width " + std::to_string(width_);
        cmd += " --height " + std::to_string(height_);
        cmd += " --framerate " + std::to_string(fps_);
        cmd += " --timeout 0";  // Run indefinitely
        cmd += " --nopreview";  // No preview window
        cmd += " --codec yuv420";  // YUV420 output
        cmd += " --output -";  // Output to stdout
        cmd += " --flush";  // Flush buffers for low latency
        cmd += " 2>/dev/null";  // Suppress stderr
        
        LOG_DEBUG("Camera command: " + cmd);
        
        // Open pipe to camera
        camera_pipe_ = popen(cmd.c_str(), "r");
        if (!camera_pipe_) {
            LOG_ERROR("Failed to start camera pipe");
            LOG_ERROR("Make sure rpicam-vid is installed: sudo apt install rpicam-apps");
            return false;
        }
        
        LOG_INFO("Camera pipe started successfully");
        
        // Allocate frame buffer for YUV420 (1.5 bytes per pixel)
        size_t yuv_size = width_ * height_ * 3 / 2;
        frame_buffer_.resize(yuv_size);
        
        LOG_INFO("Frame buffer allocated: " + std::to_string(yuv_size) + " bytes");
        
        // Wait for camera to warm up (auto-exposure, white balance)
        // Match Python picamera2 behavior
        LOG_DEBUG("Warming up camera (2 seconds for auto-exposure/white balance)...");
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));
        
        // Discard initial frames from warmup period
        LOG_DEBUG("Discarding warmup frames...");
        for (int i = 0; i < 10; i++) {
            size_t bytes_read = fread(frame_buffer_.data(), 1, frame_buffer_.size(), camera_pipe_);
            if (bytes_read != frame_buffer_.size()) {
                LOG_WARN("Warmup frame " + std::to_string(i) + " incomplete");
            }
        }
        
        LOG_INFO("Camera warmup complete and ready");
        
        initialized_ = true;
        return true;
        
    } catch (const std::exception& e) {
        LOG_ERROR("Exception during camera initialization: " + std::string(e.what()));
        release();
        return false;
    }
}

bool CameraFrameSource::getFrame(cv::Mat& frame) {
    if (!initialized_ || !camera_pipe_) {
        LOG_ERROR("CameraFrameSource not initialized");
        return false;
    }
    
    try {
        // Read YUV420 frame data from pipe
        size_t bytes_read = fread(frame_buffer_.data(), 1, frame_buffer_.size(), camera_pipe_);
        
        if (bytes_read != frame_buffer_.size()) {
            if (feof(camera_pipe_)) {
                LOG_ERROR("Camera pipe ended unexpectedly (EOF)");
            } else if (ferror(camera_pipe_)) {
                LOG_ERROR("Camera pipe read error");
            } else {
                LOG_ERROR("Incomplete frame read: " + std::to_string(bytes_read) + 
                         " / " + std::to_string(frame_buffer_.size()) + " bytes");
            }
            return false;
        }
        
        // Convert YUV420 to BGR cv::Mat
        // YUV420 layout: width*height Y plane, then width*height/4 U plane, then width*height/4 V plane
        cv::Mat yuv(height_ * 3 / 2, width_, CV_8UC1, frame_buffer_.data());
        cv::Mat bgr;
        cv::cvtColor(yuv, bgr, cv::COLOR_YUV2BGR_I420);
        
        // Scale down if enabled
        if (enable_scaling_) {
            cv::resize(bgr, frame, cv::Size(scaled_width_, scaled_height_));
        } else {
            frame = bgr;
        }
        
        return true;
        
    } catch (const std::exception& e) {
        LOG_ERROR("Exception while reading camera frame: " + std::string(e.what()));
        return false;
    }
}

void CameraFrameSource::release() {
    if (!initialized_) {
        return;
    }
    
    LOG_INFO("Releasing camera: " + device_);
    
    if (camera_pipe_) {
        pclose(camera_pipe_);
        camera_pipe_ = nullptr;
    }
    
    frame_buffer_.clear();
    initialized_ = false;
}

std::string CameraFrameSource::getName() const {
    std::string name = "CameraFrameSource (rpicam-vid pipe): " + device_ + 
                       " (" + std::to_string(width_) + "x" + std::to_string(height_) + 
                       "@" + std::to_string(fps_) + "fps)";
    if (enable_scaling_) {
        name += " -> scaled to " + std::to_string(scaled_width_) + "x" + std::to_string(scaled_height_);
    }
    return name;
}

bool CameraFrameSource::isReady() const {
    return initialized_ && camera_pipe_ != nullptr;
}

} // namespace TVLED
