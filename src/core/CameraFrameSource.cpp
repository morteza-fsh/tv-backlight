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
        
        // Build rpicam-vid command with MJPEG stream output
        std::string cmd = "rpicam-vid";
        cmd += " --camera " + std::to_string(camera_index);
        cmd += " --width " + std::to_string(width_);
        cmd += " --height " + std::to_string(height_);
        cmd += " --framerate " + std::to_string(fps_);
        cmd += " --timeout 0";  // Run indefinitely
        cmd += " --nopreview";  // No preview window
        cmd += " --codec mjpeg";  // MJPEG stream
        cmd += " --output -";  // Output to stdout
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
        
        // Allocate frame buffer for MJPEG (compressed)
        size_t buffer_size = width_ * height_;  // MJPEG is compressed
        frame_buffer_.resize(buffer_size);
        
        LOG_INFO("Frame buffer allocated: " + std::to_string(buffer_size) + " bytes");
        
        // Wait for camera to warm up
        LOG_DEBUG("Warming up camera (2 seconds)...");
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));
        
        // Discard warmup frames
        LOG_DEBUG("Discarding warmup frames...");
        for (int i = 0; i < 3; i++) {
            cv::Mat dummy;
            getFrameInternal(dummy);  // Read and discard
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

bool CameraFrameSource::getFrameInternal(cv::Mat& frame) {
    // Read MJPEG frame from pipe by finding JPEG start/end markers
    // JPEG format: starts with 0xFF 0xD8, ends with 0xFF 0xD9
    
    std::vector<uint8_t> jpeg_data;
    jpeg_data.reserve(frame_buffer_.size());
    
    bool found_start = false;
    uint8_t prev_byte = 0;
    
    while (jpeg_data.size() < frame_buffer_.size()) {
        uint8_t byte;
        size_t bytes_read = fread(&byte, 1, 1, camera_pipe_);
        
        if (bytes_read == 0) {
            return false;  // EOF or error
        }
        
        // Look for JPEG start marker (0xFF 0xD8)
        if (!found_start && prev_byte == 0xFF && byte == 0xD8) {
            found_start = true;
            jpeg_data.push_back(0xFF);
            jpeg_data.push_back(0xD8);
            prev_byte = byte;
            continue;
        }
        
        if (found_start) {
            jpeg_data.push_back(byte);
            
            // Look for JPEG end marker (0xFF 0xD9)
            if (prev_byte == 0xFF && byte == 0xD9) {
                // Complete JPEG frame found
                break;
            }
        }
        
        prev_byte = byte;
    }
    
    if (jpeg_data.empty()) {
        return false;
    }
    
    // Decode JPEG
    cv::Mat jpeg_image = cv::imdecode(jpeg_data, cv::IMREAD_COLOR);
    
    if (jpeg_image.empty()) {
        return false;
    }
    
    frame = jpeg_image;
    return true;
}

bool CameraFrameSource::getFrame(cv::Mat& frame) {
    if (!initialized_ || !camera_pipe_) {
        LOG_ERROR("CameraFrameSource not initialized");
        return false;
    }
    
    try {
        cv::Mat bgr;
        if (!getFrameInternal(bgr)) {
            LOG_ERROR("Failed to read frame from stream");
            return false;
        }
        
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

