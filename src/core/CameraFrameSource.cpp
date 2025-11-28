#include "core/CameraFrameSource.h"
#include "utils/Logger.h"
#include <opencv2/imgproc.hpp>
#include <thread>
#include <chrono>
#include <cstdlib>
#include <cstring>

namespace TVLED {

CameraFrameSource::CameraFrameSource(const std::string& device, int width, int height, int fps, int sensor_mode,
                                     const std::string& autofocus_mode, float lens_position,
                                     const std::string& awb_mode, float awb_gain_red, float awb_gain_blue,
                                     float awb_temperature, float analogue_gain, float digital_gain,
                                     int exposure_time, const std::vector<float>& color_correction_matrix,
                                     bool enable_scaling, int scaled_width, int scaled_height)
    : device_(device), width_(width), height_(height), fps_(fps), sensor_mode_(sensor_mode),
      autofocus_mode_(autofocus_mode), lens_position_(lens_position),
      awb_mode_(awb_mode), awb_gain_red_(awb_gain_red), awb_gain_blue_(awb_gain_blue),
      awb_temperature_(awb_temperature), analogue_gain_(analogue_gain), digital_gain_(digital_gain),
      exposure_time_(exposure_time), color_correction_matrix_(color_correction_matrix),
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
        
        // Add autofocus settings
        if (!autofocus_mode_.empty() && autofocus_mode_ != "default") {
            cmd += " --autofocus-mode " + autofocus_mode_;
            if (autofocus_mode_ == "manual" && lens_position_ > 0.0f) {
                cmd += " --lens-position " + std::to_string(lens_position_);
            }
        }
        
        // Add white balance settings
        if (!awb_mode_.empty() && awb_mode_ != "auto") {
            cmd += " --awb " + awb_mode_;
            // If custom mode and gains are specified, add them
            if (awb_mode_ == "custom" && awb_gain_red_ > 0.0f && awb_gain_blue_ > 0.0f) {
                cmd += " --awbgains " + std::to_string(awb_gain_red_) + "," + std::to_string(awb_gain_blue_);
            }
        }
        
        // Note: awb-temperature is not supported by rpicam-vid
        // Color temperature is implicitly set by AWB mode or custom gains
        
        // Add gain settings
        if (analogue_gain_ > 0.0f) {
            cmd += " --gain " + std::to_string(analogue_gain_);
        }
        
        // Note: digital-gain may not be supported on all rpicam-vid versions
        // Keeping the parameter but it may be ignored
        if (digital_gain_ > 0.0f) {
            LOG_WARN("digital-gain parameter may not be supported by rpicam-vid");
            // cmd += " --digital-gain " + std::to_string(digital_gain_);
        }
        
        // Add exposure time (shutter speed in microseconds)
        // Note: rpicam-vid uses --shutter for exposure time, not --exposure
        // --exposure is for exposure mode (normal, sport, etc.)
        if (exposure_time_ > 0) {
            cmd += " --shutter " + std::to_string(exposure_time_);
        }
        
        // Add color correction matrix if specified (9 values for 3x3 matrix)
        // NOTE: CCM requires explicit AWB gains to be set (awb_mode must be "custom")
        if (color_correction_matrix_.size() == 9) {
            if (awb_mode_ == "custom" && awb_gain_red_ > 0.0f && awb_gain_blue_ > 0.0f) {
                // rpicam-vid expects CCM in format: m00,m01,m02,m10,m11,m12,m20,m21,m22
                std::string ccm_str;
                for (size_t i = 0; i < color_correction_matrix_.size(); ++i) {
                    if (i > 0) ccm_str += ",";
                    ccm_str += std::to_string(color_correction_matrix_[i]);
                }
                cmd += " --ccm " + ccm_str;
            } else {
                LOG_WARN("Color correction matrix requires awb_mode='custom' with explicit AWB gains");
            }
        }
        
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
        
        // Pre-allocate frame buffer for MJPEG (compressed)
        // MJPEG typically compresses to 5-15% of raw size, reserve conservative estimate
        size_t buffer_size = width_ * height_;  // Reserve for worst-case
        frame_buffer_.reserve(buffer_size);
        
        LOG_INFO("Frame buffer capacity reserved: " + std::to_string(buffer_size) + " bytes");
        
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
    // Reuse the frame_buffer_ to avoid repeated allocations
    const size_t chunk_size = 8192;
    frame_buffer_.clear();  // Clear but keep capacity
    
    uint8_t buffer[chunk_size];
    uint8_t prev_byte = 0;
    bool found_start = false;
    int read_attempts = 0;
    const int max_read_attempts = 1000;  // Prevent infinite loops
    
    while (read_attempts++ < max_read_attempts) {
        size_t bytes_read = fread(buffer, 1, chunk_size, camera_pipe_);
        if (bytes_read == 0) {
            // Check for errors vs EOF
            if (feof(camera_pipe_)) {
                LOG_ERROR("Camera pipe reached EOF");
            } else if (ferror(camera_pipe_)) {
                LOG_ERROR("Camera pipe read error");
            }
            return false;
        }
        
        for (size_t i = 0; i < bytes_read; i++) {
            uint8_t byte = buffer[i];
            
            if (!found_start) {
                // Look for JPEG start marker (0xFF 0xD8)
                if (prev_byte == 0xFF && byte == 0xD8) {
                    found_start = true;
                    frame_buffer_.clear();
                    frame_buffer_.push_back(0xFF);
                    frame_buffer_.push_back(0xD8);
                }
            } else {
                // Collecting JPEG data
                frame_buffer_.push_back(byte);
                
                // Look for JPEG end marker (0xFF 0xD9)
                if (prev_byte == 0xFF && byte == 0xD9) {
                    // Complete JPEG frame found, decode it
                    cv::Mat img = cv::imdecode(frame_buffer_, cv::IMREAD_COLOR);
                    if (!img.empty()) {
                        frame = img;
                        return true;
                    } else {
                        // Failed to decode, reset and look for next frame
                        LOG_WARN("Failed to decode JPEG frame, size: " + std::to_string(frame_buffer_.size()));
                        found_start = false;
                        frame_buffer_.clear();
                    }
                }
            }
            
            prev_byte = byte;
        }
    }
    
    LOG_ERROR("Exceeded max read attempts without finding complete frame");
    return false;
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

