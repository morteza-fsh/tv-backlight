#include "core/CameraFrameSource.h"
#include "utils/Logger.h"
#include <opencv2/imgproc.hpp>
#include <thread>
#include <chrono>

namespace TVLED {

CameraFrameSource::CameraFrameSource(const std::string& device, int width, int height, int fps)
    : device_(device), width_(width), height_(height), fps_(fps), 
      initialized_(false), capture_(nullptr) {
}

CameraFrameSource::~CameraFrameSource() {
    release();
}

int CameraFrameSource::parseDeviceIndex() const {
    // Try to parse device string as integer index (e.g., "0", "1")
    try {
        return std::stoi(device_);
    } catch (...) {
        // If parsing fails, try to extract index from device path like "/dev/video0"
        size_t pos = device_.rfind("video");
        if (pos != std::string::npos && pos + 5 < device_.length()) {
            try {
                return std::stoi(device_.substr(pos + 5));
            } catch (...) {
                // Default to 0 if parsing fails
                return 0;
            }
        }
    }
    return 0;
}

bool CameraFrameSource::initialize() {
    LOG_INFO("Initializing camera: " + device_ + 
             " at " + std::to_string(width_) + "x" + std::to_string(height_) + 
             "@" + std::to_string(fps_) + "fps");
    
    try {
        // Parse device index from string
        int device_index = parseDeviceIndex();
        LOG_DEBUG("Using camera index: " + std::to_string(device_index));
        
        // Create VideoCapture object
        capture_ = std::make_unique<cv::VideoCapture>();
        
        // Try to open the camera
        // On Raspberry Pi, OpenCV will use V4L2 backend by default
        if (!capture_->open(device_index, cv::CAP_ANY)) {
            LOG_ERROR("Failed to open camera device: " + device_);
            capture_.reset();
            return false;
        }
        
        // Check if camera is opened
        if (!capture_->isOpened()) {
            LOG_ERROR("Camera opened but not ready: " + device_);
            capture_.reset();
            return false;
        }
        
        // Set camera properties
        bool success = true;
        
        // Set resolution
        if (!capture_->set(cv::CAP_PROP_FRAME_WIDTH, width_)) {
            LOG_WARN("Failed to set camera width to " + std::to_string(width_));
            success = false;
        }
        if (!capture_->set(cv::CAP_PROP_FRAME_HEIGHT, height_)) {
            LOG_WARN("Failed to set camera height to " + std::to_string(height_));
            success = false;
        }
        
        // Set FPS
        if (!capture_->set(cv::CAP_PROP_FPS, fps_)) {
            LOG_WARN("Failed to set camera FPS to " + std::to_string(fps_));
            success = false;
        }
        
        // Get actual camera settings (they might differ from requested)
        double actual_width = capture_->get(cv::CAP_PROP_FRAME_WIDTH);
        double actual_height = capture_->get(cv::CAP_PROP_FRAME_HEIGHT);
        double actual_fps = capture_->get(cv::CAP_PROP_FPS);
        
        LOG_INFO("Camera initialized successfully");
        LOG_INFO("Requested: " + std::to_string(width_) + "x" + std::to_string(height_) + 
                 "@" + std::to_string(fps_) + "fps");
        LOG_INFO("Actual: " + std::to_string((int)actual_width) + "x" + 
                 std::to_string((int)actual_height) + "@" + 
                 std::to_string((int)actual_fps) + "fps");
        
        // Warm up the camera by reading a few frames
        LOG_DEBUG("Warming up camera...");
        cv::Mat temp_frame;
        bool warmup_success = false;
        int max_warmup_attempts = 10;
        
        for (int i = 0; i < max_warmup_attempts; i++) {
            if (capture_->read(temp_frame) && !temp_frame.empty()) {
                warmup_success = true;
                LOG_DEBUG("Camera warmup successful on attempt " + std::to_string(i + 1));
                break;
            }
            LOG_DEBUG("Warmup attempt " + std::to_string(i + 1) + " failed, retrying...");
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        if (!warmup_success) {
            LOG_WARN("Camera warmup did not succeed, but continuing anyway");
            // Don't fail initialization - some cameras work despite failed warmup
        }
        
        // Read a few more frames to ensure camera is stable
        if (warmup_success) {
            for (int i = 0; i < 3; i++) {
                capture_->read(temp_frame);
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
        }
        
        initialized_ = true;
        return true;
        
    } catch (const std::exception& e) {
        LOG_ERROR("Exception during camera initialization: " + std::string(e.what()));
        capture_.reset();
        initialized_ = false;
        return false;
    }
}

bool CameraFrameSource::getFrame(cv::Mat& frame) {
    if (!initialized_ || !capture_) {
        LOG_ERROR("CameraFrameSource not initialized");
        return false;
    }
    
    if (!capture_->isOpened()) {
        LOG_ERROR("Camera is not opened");
        return false;
    }
    
    try {
        // Read frame from camera
        bool read_success = capture_->read(frame);
        
        if (!read_success) {
            LOG_ERROR("Failed to read frame from camera (read() returned false)");
            LOG_DEBUG("Camera backend: " + capture_->getBackendName());
            return false;
        }
        
        // Check if frame is valid
        if (frame.empty()) {
            LOG_ERROR("Camera returned empty frame");
            LOG_DEBUG("Frame dimensions: " + std::to_string(frame.cols) + "x" + std::to_string(frame.rows));
            return false;
        }
        
        // Resize if necessary (if camera doesn't support exact resolution)
        if (frame.cols != width_ || frame.rows != height_) {
            LOG_DEBUG("Resizing frame from " + std::to_string(frame.cols) + "x" + 
                     std::to_string(frame.rows) + " to " + 
                     std::to_string(width_) + "x" + std::to_string(height_));
            cv::Mat resized;
            cv::resize(frame, resized, cv::Size(width_, height_), 0, 0, cv::INTER_LINEAR);
            frame = resized;
        }
        
        return true;
        
    } catch (const std::exception& e) {
        LOG_ERROR("Exception while reading camera frame: " + std::string(e.what()));
        return false;
    }
}

void CameraFrameSource::release() {
    if (capture_ && capture_->isOpened()) {
        LOG_INFO("Releasing camera: " + device_);
        capture_->release();
    }
    capture_.reset();
    initialized_ = false;
}

std::string CameraFrameSource::getName() const {
    return "CameraFrameSource: " + device_ + 
           " (" + std::to_string(width_) + "x" + std::to_string(height_) + 
           "@" + std::to_string(fps_) + "fps)";
}

bool CameraFrameSource::isReady() const {
    return initialized_ && capture_ && capture_->isOpened();
}

} // namespace TVLED

