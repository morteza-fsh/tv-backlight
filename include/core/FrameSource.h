#pragma once

#include <opencv2/opencv.hpp>
#include <string>

namespace TVLED {

class FrameSource {
public:
    virtual ~FrameSource() = default;
    
    // Initialize the frame source
    virtual bool initialize() = 0;
    
    // Get the next frame
    virtual bool getFrame(cv::Mat& frame) = 0;
    
    // Release resources
    virtual void release() = 0;
    
    // Get source name/description
    virtual std::string getName() const = 0;
    
    // Check if source is ready
    virtual bool isReady() const = 0;
};

} // namespace TVLED

