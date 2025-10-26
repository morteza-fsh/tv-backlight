#include "core/ImageFrameSource.h"
#include "utils/Logger.h"

namespace TVLED {

ImageFrameSource::ImageFrameSource(const std::string& imagePath)
    : image_path_(imagePath), initialized_(false) {
}

bool ImageFrameSource::initialize() {
    image_ = cv::imread(image_path_);
    
    if (image_.empty()) {
        LOG_ERROR("Failed to load image: " + image_path_);
        initialized_ = false;
        return false;
    }
    
    LOG_INFO("Loaded image: " + image_path_ + " (" + 
             std::to_string(image_.cols) + "x" + std::to_string(image_.rows) + ")");
    
    initialized_ = true;
    return true;
}

bool ImageFrameSource::getFrame(cv::Mat& frame) {
    if (!initialized_) {
        LOG_ERROR("ImageFrameSource not initialized");
        return false;
    }
    
    // Return a copy of the image
    frame = image_.clone();
    return true;
}

void ImageFrameSource::release() {
    image_.release();
    initialized_ = false;
    LOG_INFO("ImageFrameSource released");
}

std::string ImageFrameSource::getName() const {
    return "ImageFrameSource: " + image_path_;
}

bool ImageFrameSource::isReady() const {
    return initialized_;
}

} // namespace TVLED

