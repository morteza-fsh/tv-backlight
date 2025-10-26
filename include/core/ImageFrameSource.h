#pragma once

#include "core/FrameSource.h"
#include <string>

namespace TVLED {

class ImageFrameSource : public FrameSource {
public:
    explicit ImageFrameSource(const std::string& imagePath);
    ~ImageFrameSource() override = default;
    
    bool initialize() override;
    bool getFrame(cv::Mat& frame) override;
    void release() override;
    std::string getName() const override;
    bool isReady() const override;

private:
    std::string image_path_;
    cv::Mat image_;
    bool initialized_;
};

} // namespace TVLED

