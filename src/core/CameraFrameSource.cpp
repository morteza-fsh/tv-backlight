#include "core/CameraFrameSource.h"
#include "utils/Logger.h"
#include <opencv2/imgproc.hpp>
#include <libcamera/libcamera.h>
#include <thread>
#include <chrono>
#include <sys/mman.h>

using namespace libcamera;

namespace TVLED {

CameraFrameSource::CameraFrameSource(const std::string& device, int width, int height, int fps)
    : device_(device), width_(width), height_(height), fps_(fps), 
      initialized_(false), stream_(nullptr), stop_capture_(false) {
}

CameraFrameSource::~CameraFrameSource() {
    release();
}

int CameraFrameSource::parseCameraIndex() const {
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
                return 0;
            }
        }
    }
    return 0;
}

bool CameraFrameSource::initialize() {
    LOG_INFO("Initializing camera with libcamera: " + device_ + 
             " at " + std::to_string(width_) + "x" + std::to_string(height_) + 
             "@" + std::to_string(fps_) + "fps");
    
    try {
        // Create camera manager
        camera_manager_ = std::make_unique<CameraManager>();
        int ret = camera_manager_->start();
        if (ret) {
            LOG_ERROR("Failed to start camera manager");
            return false;
        }
        
        // List available cameras
        auto cameras = camera_manager_->cameras();
        if (cameras.empty()) {
            LOG_ERROR("No cameras available");
            camera_manager_->stop();
            return false;
        }
        
        LOG_INFO("Found " + std::to_string(cameras.size()) + " camera(s)");
        
        // Get camera index
        int camera_index = parseCameraIndex();
        if (camera_index >= static_cast<int>(cameras.size())) {
            LOG_WARN("Camera index " + std::to_string(camera_index) + 
                    " out of range, using camera 0");
            camera_index = 0;
        }
        
        // Get the camera
        camera_ = cameras[camera_index];
        LOG_INFO("Using camera: " + camera_->id());
        
        // Acquire the camera
        if (camera_->acquire()) {
            LOG_ERROR("Failed to acquire camera");
            camera_manager_->stop();
            return false;
        }
        
        // Generate camera configuration
        std::unique_ptr<CameraConfiguration> config = 
            camera_->generateConfiguration({StreamRole::VideoRecording});
        
        if (!config) {
            LOG_ERROR("Failed to generate camera configuration");
            camera_->release();
            camera_manager_->stop();
            return false;
        }
        
        // Configure the stream
        StreamConfiguration &stream_config = config->at(0);
        
        // Set pixel format (prefer YUV420 for compatibility)
        stream_config.pixelFormat = PixelFormat::fromString("YUV420");
        stream_config.size.width = width_;
        stream_config.size.height = height_;
        
        // Set buffer count
        stream_config.bufferCount = 4;
        
        LOG_DEBUG("Requested format: " + stream_config.pixelFormat.toString() + 
                 " " + std::to_string(width_) + "x" + std::to_string(height_));
        
        // Validate configuration
        CameraConfiguration::Status validation = config->validate();
        if (validation == CameraConfiguration::Invalid) {
            LOG_ERROR("Camera configuration invalid");
            camera_->release();
            camera_manager_->stop();
            return false;
        }
        
        if (validation == CameraConfiguration::Adjusted) {
            LOG_WARN("Camera configuration adjusted");
            LOG_INFO("Actual format: " + stream_config.pixelFormat.toString() + 
                    " " + std::to_string(stream_config.size.width) + "x" + 
                    std::to_string(stream_config.size.height));
        }
        
        // Apply configuration
        if (camera_->configure(config.get())) {
            LOG_ERROR("Failed to configure camera");
            camera_->release();
            camera_manager_->stop();
            return false;
        }
        
        // Store stream pointer
        stream_ = stream_config.stream();
        
        // Allocate buffers
        allocator_ = std::make_unique<FrameBufferAllocator>(camera_);
        if (allocator_->allocate(stream_) < 0) {
            LOG_ERROR("Failed to allocate buffers");
            camera_->release();
            camera_manager_->stop();
            return false;
        }
        
        LOG_INFO("Allocated " + std::to_string(allocator_->buffers(stream_).size()) + " buffers");
        
        // Create requests
        for (const std::unique_ptr<FrameBuffer> &buffer : allocator_->buffers(stream_)) {
            std::unique_ptr<Request> request = camera_->createRequest();
            if (!request) {
                LOG_ERROR("Failed to create request");
                camera_->release();
                camera_manager_->stop();
                return false;
            }
            
            if (request->addBuffer(stream_, buffer.get())) {
                LOG_ERROR("Failed to add buffer to request");
                camera_->release();
                camera_manager_->stop();
                return false;
            }
            
            // Set controls (optional - for exposure/white balance)
            // request->controls().set(controls::ExposureTime, ...);
            
            requests_.push_back(std::move(request));
        }
        
        // Connect request completed signal
        camera_->requestCompleted.connect(this, &CameraFrameSource::onRequestComplete);
        
        // Start camera
        if (camera_->start()) {
            LOG_ERROR("Failed to start camera");
            camera_->release();
            camera_manager_->stop();
            return false;
        }
        
        // Queue all requests
        for (std::unique_ptr<Request> &request : requests_) {
            camera_->queueRequest(request.get());
        }
        
        LOG_INFO("Camera started successfully");
        
        // Warm up - wait for auto-exposure/white balance to settle
        // This matches the Python picamera2 behavior (time.sleep(2))
        LOG_DEBUG("Warming up camera (allowing auto-exposure/white balance to settle)...");
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));
        
        // Clear any queued frames from warmup
        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            while (!frame_queue_.empty()) {
                frame_queue_.pop();
            }
        }
        
        LOG_INFO("Camera warmup complete");
        
        initialized_ = true;
        return true;
        
    } catch (const std::exception& e) {
        LOG_ERROR("Exception during camera initialization: " + std::string(e.what()));
        release();
        return false;
    }
}

void CameraFrameSource::onRequestComplete(Request *request) {
    if (request->status() == Request::RequestCancelled) {
        return;
    }
    
    // Convert frame to cv::Mat
    cv::Mat frame = convertFrameToMat(request);
    
    if (!frame.empty()) {
        // Add to queue
        std::lock_guard<std::mutex> lock(queue_mutex_);
        if (frame_queue_.size() < 5) {  // Limit queue size
            frame_queue_.push(frame.clone());
            queue_cv_.notify_one();
        }
    }
    
    // Requeue the request if camera is still running
    if (!stop_capture_ && camera_ && camera_->state() == Camera::CameraRunning) {
        request->reuse(Request::ReuseBuffers);
        camera_->queueRequest(request);
    }
}

cv::Mat CameraFrameSource::convertFrameToMat(Request *request) {
    const FrameBuffer *buffer = request->buffers().begin()->second;
    const FrameBuffer::Plane &plane = buffer->planes()[0];
    
    // Map the buffer
    void *memory = mmap(nullptr, plane.length, PROT_READ, MAP_SHARED, 
                       plane.fd.get(), 0);
    if (memory == MAP_FAILED) {
        LOG_ERROR("Failed to mmap frame buffer");
        return cv::Mat();
    }
    
    // Get stream configuration
    const StreamConfiguration &cfg = stream_->configuration();
    
    // Create cv::Mat from YUV420 data
    cv::Mat yuv(cfg.size.height * 3 / 2, cfg.size.width, CV_8UC1, memory);
    cv::Mat bgr;
    
    // Convert YUV420 to BGR
    cv::cvtColor(yuv, bgr, cv::COLOR_YUV2BGR_I420);
    
    // Clone the data before unmapping
    cv::Mat result = bgr.clone();
    
    // Unmap the buffer
    munmap(memory, plane.length);
    
    // Resize if necessary
    if (result.cols != width_ || result.rows != height_) {
        cv::Mat resized;
        cv::resize(result, resized, cv::Size(width_, height_), 0, 0, cv::INTER_LINEAR);
        return resized;
    }
    
    return result;
}

bool CameraFrameSource::getFrame(cv::Mat& frame) {
    if (!initialized_ || !camera_) {
        LOG_ERROR("CameraFrameSource not initialized");
        return false;
    }
    
    // Wait for a frame with timeout
    std::unique_lock<std::mutex> lock(queue_mutex_);
    if (queue_cv_.wait_for(lock, std::chrono::milliseconds(500), 
                           [this]{ return !frame_queue_.empty() || stop_capture_; })) {
        if (!frame_queue_.empty()) {
            frame = frame_queue_.front();
            frame_queue_.pop();
            return true;
        }
    }
    
    LOG_ERROR("Timeout waiting for frame");
    return false;
}

void CameraFrameSource::release() {
    if (!initialized_) {
        return;
    }
    
    LOG_INFO("Releasing camera: " + device_);
    
    stop_capture_ = true;
    queue_cv_.notify_all();
    
    if (camera_) {
        if (camera_->state() == Camera::CameraRunning) {
            camera_->stop();
        }
        camera_->requestCompleted.disconnect(this, &CameraFrameSource::onRequestComplete);
        camera_->release();
        camera_.reset();
    }
    
    requests_.clear();
    allocator_.reset();
    
    if (camera_manager_) {
        camera_manager_->stop();
        camera_manager_.reset();
    }
    
    // Clear frame queue
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        while (!frame_queue_.empty()) {
            frame_queue_.pop();
        }
    }
    
    initialized_ = false;
}

std::string CameraFrameSource::getName() const {
    return "CameraFrameSource (libcamera): " + device_ + 
           " (" + std::to_string(width_) + "x" + std::to_string(height_) + 
           "@" + std::to_string(fps_) + "fps)";
}

bool CameraFrameSource::isReady() const {
    return initialized_ && camera_ && camera_->state() == Camera::CameraRunning;
}

} // namespace TVLED
