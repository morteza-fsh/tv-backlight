#pragma once

#include "core/Config.h"
#include "core/FrameSource.h"
#include "processing/BezierCurve.h"
#include "processing/CoonsPatching.h"
#include "processing/ColorExtractor.h"
#include "communication/LEDLayout.h"
#include "communication/HyperHDRClient.h"
#include "communication/USBController.h"
#include <memory>
#include <atomic>

namespace TVLED {

class LEDController {
public:
    explicit LEDController(const Config& config);
    ~LEDController();
    
    // Initialize all subsystems
    bool initialize();
    
    // Run the main processing loop
    // Returns number of frames processed
    int run();
    
    // Stop the processing loop
    void stop();
    
    // Process a single frame (for debugging)
    bool processSingleFrame(bool saveDebugImages = false);

private:
    // Setup methods
    bool setupFrameSource();
    bool setupBezierCurves();
    bool setupCoonsPatching(int imageWidth, int imageHeight);
    bool setupColorExtractor();
    bool setupLEDLayout();
    bool setupHyperHDRClient();
    bool setupUSBController();
    
    // Processing
    bool processFrame(const cv::Mat& frame, std::vector<cv::Vec3b>& colors);
    
    // Debug output
    void saveDebugBoundaries(const cv::Mat& frame);
    void saveColorGrid(const std::vector<cv::Vec3b>& colors);
    void saveRectangleImage(const cv::Mat& frame);
    
    Config config_;
    std::unique_ptr<FrameSource> frame_source_;
    std::unique_ptr<CoonsPatching> coons_patching_;
    std::unique_ptr<ColorExtractor> color_extractor_;
    std::unique_ptr<LEDLayout> led_layout_;
    std::unique_ptr<HyperHDRClient> hyperhdr_client_;
    std::unique_ptr<USBController> usb_controller_;
    
    BezierCurve top_bezier_, right_bezier_, bottom_bezier_, left_bezier_;
    
    std::vector<std::vector<cv::Point>> cell_polygons_;
    
    std::atomic<bool> running_;
    bool initialized_;
};

} // namespace TVLED

