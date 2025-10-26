#pragma once

#include "core/FrameSource.h"
#include <string>

namespace TVLED {

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
    
    // Platform-specific camera handle
    void* camera_handle_;  // Will be cast to appropriate type on platform
};

} // namespace TVLED

