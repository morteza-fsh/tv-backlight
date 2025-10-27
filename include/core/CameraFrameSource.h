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
    CameraFrameSource(const std::string& device, int width, int height, int fps);
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
    bool initialized_;
    
    // For piped camera input
    FILE* camera_pipe_;
    std::vector<uint8_t> frame_buffer_;
    
    // Helper methods
    int parseCameraIndex() const;
};

} // namespace TVLED
