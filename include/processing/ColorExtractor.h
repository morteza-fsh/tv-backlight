#pragma once

#include <opencv2/opencv.hpp>
#include <vector>

namespace TVLED {

class ColorExtractor {
public:
    ColorExtractor() : enable_parallel_(true), masks_precomputed_(false), method_("mean"),
                       gamma_enabled_(false), gamma_red_(2.2), gamma_green_(2.2), gamma_blue_(2.2) {
        buildGammaLUT();
    }
    
    // Extract colors from regions defined by polygons
    // Returns RGB colors (converted from OpenCV's BGR)
    std::vector<cv::Vec3b> extractColors(const cv::Mat& frame,
                                         const std::vector<std::vector<cv::Point>>& polygons);
    
    // Pre-compute masks for polygons (optimization: avoid per-frame mask creation)
    // Call this once after polygons are created with known frame dimensions
    void precomputeMasks(const std::vector<std::vector<cv::Point>>& polygons,
                        int frame_width, int frame_height);
    
    // Clear pre-computed masks (call when polygons change)
    void clearMasks() {
        cached_masks_.clear();
        cached_bboxes_.clear();
        masks_precomputed_ = false;
    }
    
    void setParallelProcessing(bool enable) { enable_parallel_ = enable; }
    bool isParallelProcessingEnabled() const { return enable_parallel_; }
    
    // Set color extraction method: "mean" or "dominant"
    void setMethod(const std::string& method) { method_ = method; }
    std::string getMethod() const { return method_; }
    
    // Gamma correction settings
    void setGammaCorrection(bool enabled, double gamma_r, double gamma_g, double gamma_b) {
        gamma_enabled_ = enabled;
        gamma_red_ = gamma_r;
        gamma_green_ = gamma_g;
        gamma_blue_ = gamma_b;
        buildGammaLUT();
    }
    
    void enableGammaCorrection(bool enabled) { gamma_enabled_ = enabled; }
    bool isGammaCorrectionEnabled() const { return gamma_enabled_; }

private:
    // Extract color from a single polygon region
    cv::Vec3b extractSingleColor(const cv::Mat& frame,
                                 const std::vector<cv::Point>& polygon,
                                 const cv::Rect& bbox);
    
    // Extract color using pre-computed mask (faster)
    cv::Vec3b extractSingleColorWithMask(const cv::Mat& frame,
                                        const cv::Mat& mask,
                                        const cv::Rect& bbox);
    
    // Extract mean color (average of all pixels)
    cv::Vec3b extractMeanColor(const cv::Mat& frame,
                               const cv::Mat& mask,
                               const cv::Rect& bbox);
    
    // Extract dominant color using k-means clustering
    cv::Vec3b extractDominantColor(const cv::Mat& frame,
                                   const cv::Mat& mask,
                                   const cv::Rect& bbox);
    
    // Gamma correction utilities
    void buildGammaLUT();
    cv::Vec3b applyGammaCorrection(const cv::Vec3b& color) const;
    
    bool enable_parallel_;
    bool masks_precomputed_;
    std::string method_;  // "mean" or "dominant"
    std::vector<cv::Mat> cached_masks_;
    std::vector<cv::Rect> cached_bboxes_;
    
    // Gamma correction settings
    bool gamma_enabled_;
    double gamma_red_;
    double gamma_green_;
    double gamma_blue_;
    
    // Look-up tables for fast gamma correction (256 values for each channel)
    std::vector<uchar> gamma_lut_red_;
    std::vector<uchar> gamma_lut_green_;
    std::vector<uchar> gamma_lut_blue_;
};

} // namespace TVLED

