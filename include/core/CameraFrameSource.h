#pragma once

#include "core/FrameSource.h"
#include <opencv2/core.hpp>
#include <string>
#include <memory>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <queue>

// Forward declarations for libcamera (to avoid exposing in header)
namespace libcamera {
    class CameraManager;
    class Camera;
    class FrameBufferAllocator;
    class Request;
    class Stream;
    class ControlList;
}

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
    
    // libcamera objects
    std::unique_ptr<libcamera::CameraManager> camera_manager_;
    std::shared_ptr<libcamera::Camera> camera_;
    std::unique_ptr<libcamera::FrameBufferAllocator> allocator_;
    std::vector<std::unique_ptr<libcamera::Request>> requests_;
    libcamera::Stream *stream_;
    
    // Frame queue for captured frames
    std::queue<cv::Mat> frame_queue_;
    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    bool stop_capture_;
    
    // Helper methods
    int parseCameraIndex() const;
    void onRequestComplete(libcamera::Request *request);
    cv::Mat convertFrameToMat(libcamera::Request *request);
};

} // namespace TVLED

