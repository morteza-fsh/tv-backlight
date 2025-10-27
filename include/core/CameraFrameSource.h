#pragma once

#include "core/FrameSource.h"
#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>
#include <string>
#include <memory>
#include <cstdio>

namespace TVLED {

/**
 * Simple camera frame source using rpicam-vid piped to OpenCV
 * This is the simplest and lowest-latency approach for Raspberry Pi cameras
 */
class CameraFrameSource : public FrameSource {
public:
    CameraFrameSource(const std::string& device, int width, int height, int fps, int sensor_mode = -1,
                     bool enable_scaling = true, int scaled_width = 960, int scaled_height = 540);
    ~CameraFrameSource() override;
    
    bool initialize() override;
    bool getFrame(cv::Mat& frame) override;
    void release() override;
    std::string getName() const override;
    bool isReady() const override;

private:
    std::string device_;
    int width_;
    int height_;
    int fps_;
    int sensor_mode_;
    bool initialized_;
    
    // Scaling configuration
    bool enable_scaling_;
    int scaled_width_;
    int scaled_height_;
    
    // For piped camera input
    FILE* camera_pipe_;
    std::vector<uint8_t> frame_buffer_;
    std::string temp_file_;  // Temporary file for frame storage
    
    // Helper methods
    int parseCameraIndex() const;
    bool getFrameInternal(cv::Mat& frame);  // Internal frame reading
};

} // namespace TVLED
