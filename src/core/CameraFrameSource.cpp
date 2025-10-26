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
        // On Raspberry Pi, try V4L2 backend first, which is most compatible
        // For Raspberry Pi Camera Module, you may need to enable legacy camera support
        // or use libcamera-vid to create a V4L2 device
        bool opened = false;
        
        // Try V4L2 backend first (most common)
        if (capture_->open(device_index, cv::CAP_V4L2)) {
            LOG_INFO("Camera opened with V4L2 backend");
            opened = true;
        }
        // Fallback to any available backend
        else if (capture_->open(device_index, cv::CAP_ANY)) {
            LOG_INFO("Camera opened with default backend");
            opened = true;
        }
        
        if (!opened) {
            LOG_ERROR("Failed to open camera device: " + device_);
            LOG_ERROR("Make sure the camera is connected and V4L2 device exists");
            LOG_ERROR("For Raspberry Pi Camera Module, you may need:");
            LOG_ERROR("  1. Enable legacy camera: sudo raspi-config -> Interface Options -> Legacy Camera");
            LOG_ERROR("  2. Or use: v4l2-ctl --list-devices to find the correct device");
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
        
        // For V4L2, set the format first (helps with Raspberry Pi cameras)
        // Try MJPEG format which is widely supported
        if (!capture_->set(cv::CAP_PROP_FOURCC, cv::VideoWriter::fourcc('M','J','P','G'))) {
            LOG_DEBUG("Could not set MJPEG format, trying YUYV");
            if (!capture_->set(cv::CAP_PROP_FOURCC, cv::VideoWriter::fourcc('Y','U','Y','V'))) {
                LOG_DEBUG("Could not set YUYV format either, using default");
            }
        }
        
        // Set buffer size (helps with some cameras)
        capture_->set(cv::CAP_PROP_BUFFERSIZE, 1);
        
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
        
        // Set autofocus (if supported)
        capture_->set(cv::CAP_PROP_AUTOFOCUS, 1);
        
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
        
        // Warm up the camera by reading frames
        // Match picamera2 behavior: wait ~2 seconds for auto-exposure/white balance to settle
        LOG_DEBUG("Warming up camera (allowing auto-exposure/white balance to settle)...");
        cv::Mat temp_frame;
        bool warmup_success = false;
        
        // First, give the camera some initial time to wake up
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        // Try to get the first valid frame (up to 20 attempts over 2 seconds)
        int max_warmup_attempts = 20;
        for (int i = 0; i < max_warmup_attempts; i++) {
            bool read_result = capture_->read(temp_frame);
            bool frame_valid = !temp_frame.empty();
            
            if (i == 0) {
                // First attempt - provide detailed diagnostics
                LOG_DEBUG("First read attempt: read=" + std::string(read_result ? "success" : "failed") + 
                         ", frame_empty=" + std::string(frame_valid ? "no" : "yes"));
                if (!frame_valid && !temp_frame.empty()) {
                    LOG_DEBUG("Frame size: " + std::to_string(temp_frame.cols) + "x" + std::to_string(temp_frame.rows));
                }
            }
            
            if (read_result && frame_valid) {
                warmup_success = true;
                LOG_DEBUG("Got first valid frame on attempt " + std::to_string(i + 1));
                LOG_DEBUG("Frame size: " + std::to_string(temp_frame.cols) + "x" + std::to_string(temp_frame.rows));
                break;
            }
            LOG_DEBUG("Warmup attempt " + std::to_string(i + 1) + " failed, retrying...");
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        if (!warmup_success) {
            LOG_ERROR("Failed to read any frames during warmup after 2 seconds");
            LOG_ERROR("Camera may not be functioning properly");
            LOG_ERROR("");
            LOG_ERROR("=== TROUBLESHOOTING ===");
            LOG_ERROR("This usually means OpenCV cannot access the camera through V4L2.");
            LOG_ERROR("");
            LOG_ERROR("For Raspberry Pi Camera Module on modern Raspberry Pi OS:");
            LOG_ERROR("  1. Check if legacy camera is enabled:");
            LOG_ERROR("     sudo raspi-config -> Interface Options -> Legacy Camera -> Enable");
            LOG_ERROR("     Then reboot: sudo reboot");
            LOG_ERROR("");
            LOG_ERROR("  2. Or check if libcamera is working:");
            LOG_ERROR("     libcamera-hello --list-cameras");
            LOG_ERROR("     libcamera-still -o test.jpg");
            LOG_ERROR("");
            LOG_ERROR("  3. Check available V4L2 devices:");
            LOG_ERROR("     v4l2-ctl --list-devices");
            LOG_ERROR("     v4l2-ctl -d /dev/video0 --list-formats-ext");
            LOG_ERROR("");
            LOG_ERROR("For USB cameras:");
            LOG_ERROR("  1. Check camera is detected: lsusb");
            LOG_ERROR("  2. Check permissions: ls -l /dev/video0");
            LOG_ERROR("  3. Try: sudo usermod -a -G video $USER");
            LOG_ERROR("======================");
            capture_.reset();
            initialized_ = false;
            return false;
        }
        
        // Continue reading frames for another 1.5 seconds to allow camera to stabilize
        // This matches the 2-second delay in the Python picamera2 code
        LOG_DEBUG("Camera responding, continuing warmup for stabilization...");
        auto warmup_end = std::chrono::steady_clock::now() + std::chrono::milliseconds(1500);
        int frames_read = 0;
        
        while (std::chrono::steady_clock::now() < warmup_end) {
            if (capture_->read(temp_frame) && !temp_frame.empty()) {
                frames_read++;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        
        LOG_INFO("Camera warmup complete, read " + std::to_string(frames_read) + " stabilization frames");
        
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

