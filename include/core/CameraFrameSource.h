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
                     const std::string& autofocus_mode = "default", float lens_position = 0.0f,
                     const std::string& awb_mode = "auto", float awb_gain_red = 0.0f, float awb_gain_blue = 0.0f,
                     float awb_temperature = 0.0f, float analogue_gain = 0.0f, float digital_gain = 0.0f,
                     int exposure_time = 0, const std::vector<float>& color_correction_matrix = {},
                     bool enable_scaling = true, int scaled_width = 960, int scaled_height = 540,
                     bool flip_horizontal = false, bool flip_vertical = false);
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
    std::string autofocus_mode_;
    float lens_position_;
    std::string awb_mode_;
    float awb_gain_red_;
    float awb_gain_blue_;
    float awb_temperature_;
    float analogue_gain_;
    float digital_gain_;
    int exposure_time_;
    std::vector<float> color_correction_matrix_;
    bool initialized_;
    
    // Scaling configuration
    bool enable_scaling_;
    int scaled_width_;
    int scaled_height_;
    
    // Flip configuration
    bool flip_horizontal_;
    bool flip_vertical_;
    
    // For piped camera input
    FILE* camera_pipe_;
    std::vector<uint8_t> frame_buffer_;
    std::string temp_file_;  // Temporary file for frame storage
    
    // Helper methods
    int parseCameraIndex() const;
    bool getFrameInternal(cv::Mat& frame);  // Internal frame reading
};

} // namespace TVLED
