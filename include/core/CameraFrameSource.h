#pragma once

#include "core/FrameSource.h"
#include <opencv2/videoio.hpp>
#include <string>
#include <memory>

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
    
    // OpenCV VideoCapture for camera access
    std::unique_ptr<cv::VideoCapture> capture_;
    
    // Helper to parse device string to camera index
    int parseDeviceIndex() const;
};

} // namespace TVLED

