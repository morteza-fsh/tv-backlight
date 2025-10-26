#pragma once

#include <opencv2/opencv.hpp>
#include <vector>
#include <memory>

namespace TVLED {

// Structure to cache cumulative arc lengths for fast interpolation
class PolylineCache {
public:
    explicit PolylineCache(const std::vector<cv::Point2f>& poly);
    
    // Fast interpolation using cached lengths
    cv::Point2f interp(double t) const;
    
    double getTotalLength() const { return total_length_; }

private:
    std::vector<double> cumulative_lengths_;
    double total_length_;
    const std::vector<cv::Point2f>* poly_ptr_;
};

class CoonsPatching {
public:
    CoonsPatching() = default;
    
    // Initialize with boundary curves
    // Curves should be: top (L->R), right (T->B), bottom (L->R), left (T->B)
    bool initialize(const std::vector<cv::Point2f>& top,
                   const std::vector<cv::Point2f>& right,
                   const std::vector<cv::Point2f>& bottom,
                   const std::vector<cv::Point2f>& left,
                   int imageWidth, int imageHeight);
    
    // Coons patch interpolation at parameter (u, v) where u, v ∈ [0, 1]
    cv::Point2f interpolate(double u, double v) const;
    
    // Build a curved cell polygon for a grid cell
    std::vector<cv::Point> buildCellPolygon(double u0, double u1, 
                                            double v0, double v1, 
                                            int samples = 40) const;
    
    // Get corner points
    cv::Point2f getCorner(int index) const;
    
    bool isInitialized() const { return initialized_; }

private:
    // Boundary interpolation functions
    cv::Point2f C_top(double u) const;
    cv::Point2f C_bottom(double u) const;
    cv::Point2f D_left(double v) const;
    cv::Point2f D_right(double v) const;
    
    std::vector<cv::Point2f> top_pts_;
    std::vector<cv::Point2f> right_pts_;
    std::vector<cv::Point2f> bottom_pts_;
    std::vector<cv::Point2f> left_pts_;
    
    // Corner points (TL, TR, BR, BL)
    cv::Point2f P00_, P10_, P11_, P01_;
    
    // Cached polylines for fast interpolation
    std::unique_ptr<PolylineCache> top_cache_;
    std::unique_ptr<PolylineCache> bottom_cache_;
    std::unique_ptr<PolylineCache> left_cache_;
    std::unique_ptr<PolylineCache> right_cache_;
    
    int W_, H_;
    bool initialized_;
};

} // namespace TVLED

