#pragma once

#include <opencv2/opencv.hpp>
#include <vector>

namespace TVLED {

// Forward declaration
class CoonsPatching;

class ColorExtractor {
public:
    ColorExtractor() : enable_parallel_(true) {}
    
    // Extract dominant colors from regions defined by polygons
    // Returns RGB colors (converted from OpenCV's BGR)
    std::vector<cv::Vec3b> extractColors(const cv::Mat& frame,
                                         const std::vector<std::vector<cv::Point>>& polygons);
    
    // Extract colors from edge slices (for TV backlight mode)
    // Returns RGB colors for top, bottom, left, right edge regions
    std::vector<cv::Vec3b> extractEdgeSliceColors(
        const cv::Mat& frame,
        const CoonsPatching& coons,
        int horizontal_slices,
        int vertical_slices,
        float horizontal_coverage_percent,
        float vertical_coverage_percent,
        int polygon_samples = 15);
    
    void setParallelProcessing(bool enable) { enable_parallel_ = enable; }
    bool isParallelProcessingEnabled() const { return enable_parallel_; }

private:
    // Extract dominant color from a single polygon region
    cv::Vec3b extractSingleColor(const cv::Mat& frame,
                                 const std::vector<cv::Point>& polygon,
                                 const cv::Rect& bbox);
    
    bool enable_parallel_;
};

} // namespace TVLED

