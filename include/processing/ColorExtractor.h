#pragma once

#include <opencv2/opencv.hpp>
#include <vector>

namespace TVLED {

// Structure to hold gamma values for one corner
struct CornerGamma {
    double gamma_red = 2.2;
    double gamma_green = 2.2;
    double gamma_blue = 2.2;
    std::vector<uchar> lut_red;
    std::vector<uchar> lut_green;
    std::vector<uchar> lut_blue;
};

// Structure to hold LED count information for edges
struct LEDCounts {
    int top = 0;
    int bottom = 0;
    int left = 0;
    int right = 0;
    
    int getTotalLEDs() const {
        return top + bottom + left + right;
    }
};

class ColorExtractor {
public:
    ColorExtractor() : enable_parallel_(true), masks_precomputed_(false), method_("mean"),
                       gamma_enabled_(false) {
        // Initialize default gamma for backward compatibility
        corner_gamma_top_left_.gamma_red = corner_gamma_top_left_.gamma_green = corner_gamma_top_left_.gamma_blue = 2.2;
        corner_gamma_top_center_ = corner_gamma_top_right_ = corner_gamma_right_center_ = 
        corner_gamma_bottom_right_ = corner_gamma_bottom_center_ = corner_gamma_bottom_left_ = 
        corner_gamma_left_center_ = corner_gamma_top_left_;
        buildAllGammaLUTs();
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
    
    // Legacy gamma correction (applies to all control points)
    void setGammaCorrection(bool enabled, double gamma_r, double gamma_g, double gamma_b) {
        gamma_enabled_ = enabled;
        corner_gamma_top_left_.gamma_red = corner_gamma_top_center_.gamma_red = corner_gamma_top_right_.gamma_red = 
        corner_gamma_right_center_.gamma_red = corner_gamma_bottom_right_.gamma_red = corner_gamma_bottom_center_.gamma_red =
        corner_gamma_bottom_left_.gamma_red = corner_gamma_left_center_.gamma_red = gamma_r;
        
        corner_gamma_top_left_.gamma_green = corner_gamma_top_center_.gamma_green = corner_gamma_top_right_.gamma_green = 
        corner_gamma_right_center_.gamma_green = corner_gamma_bottom_right_.gamma_green = corner_gamma_bottom_center_.gamma_green =
        corner_gamma_bottom_left_.gamma_green = corner_gamma_left_center_.gamma_green = gamma_g;
        
        corner_gamma_top_left_.gamma_blue = corner_gamma_top_center_.gamma_blue = corner_gamma_top_right_.gamma_blue = 
        corner_gamma_right_center_.gamma_blue = corner_gamma_bottom_right_.gamma_blue = corner_gamma_bottom_center_.gamma_blue =
        corner_gamma_bottom_left_.gamma_blue = corner_gamma_left_center_.gamma_blue = gamma_b;
        
        buildAllGammaLUTs();
    }
    
    // 8-point gamma correction (4 corners + 4 edge centers)
    void setEightPointGammaCorrection(bool enabled,
                                     double tl_r, double tl_g, double tl_b,
                                     double tc_r, double tc_g, double tc_b,
                                     double tr_r, double tr_g, double tr_b,
                                     double rc_r, double rc_g, double rc_b,
                                     double br_r, double br_g, double br_b,
                                     double bc_r, double bc_g, double bc_b,
                                     double bl_r, double bl_g, double bl_b,
                                     double lc_r, double lc_g, double lc_b) {
        gamma_enabled_ = enabled;
        corner_gamma_top_left_.gamma_red = tl_r;
        corner_gamma_top_left_.gamma_green = tl_g;
        corner_gamma_top_left_.gamma_blue = tl_b;
        
        corner_gamma_top_center_.gamma_red = tc_r;
        corner_gamma_top_center_.gamma_green = tc_g;
        corner_gamma_top_center_.gamma_blue = tc_b;
        
        corner_gamma_top_right_.gamma_red = tr_r;
        corner_gamma_top_right_.gamma_green = tr_g;
        corner_gamma_top_right_.gamma_blue = tr_b;
        
        corner_gamma_right_center_.gamma_red = rc_r;
        corner_gamma_right_center_.gamma_green = rc_g;
        corner_gamma_right_center_.gamma_blue = rc_b;
        
        corner_gamma_bottom_right_.gamma_red = br_r;
        corner_gamma_bottom_right_.gamma_green = br_g;
        corner_gamma_bottom_right_.gamma_blue = br_b;
        
        corner_gamma_bottom_center_.gamma_red = bc_r;
        corner_gamma_bottom_center_.gamma_green = bc_g;
        corner_gamma_bottom_center_.gamma_blue = bc_b;
        
        corner_gamma_bottom_left_.gamma_red = bl_r;
        corner_gamma_bottom_left_.gamma_green = bl_g;
        corner_gamma_bottom_left_.gamma_blue = bl_b;
        
        corner_gamma_left_center_.gamma_red = lc_r;
        corner_gamma_left_center_.gamma_green = lc_g;
        corner_gamma_left_center_.gamma_blue = lc_b;
        
        buildAllGammaLUTs();
    }
    
    // Set LED layout for corner-based gamma calculation
    void setLEDLayout(int top, int bottom, int left, int right) {
        led_counts_.top = top;
        led_counts_.bottom = bottom;
        led_counts_.left = left;
        led_counts_.right = right;
    }
    
    void enableGammaCorrection(bool enabled) { gamma_enabled_ = enabled; }
    bool isGammaCorrectionEnabled() const { return gamma_enabled_; }

private:
    // Extract color from a single polygon region
    cv::Vec3b extractSingleColor(const cv::Mat& frame,
                                 const std::vector<cv::Point>& polygon,
                                 const cv::Rect& bbox,
                                 int led_index = -1);
    
    // Extract color using pre-computed mask (faster)
    cv::Vec3b extractSingleColorWithMask(const cv::Mat& frame,
                                        const cv::Mat& mask,
                                        const cv::Rect& bbox,
                                        int led_index = -1);
    
    // Extract mean color (average of all pixels)
    cv::Vec3b extractMeanColor(const cv::Mat& frame,
                               const cv::Mat& mask,
                               const cv::Rect& bbox,
                               int led_index = -1);
    
    // Extract dominant color using k-means clustering
    cv::Vec3b extractDominantColor(const cv::Mat& frame,
                                   const cv::Mat& mask,
                                   const cv::Rect& bbox,
                                   int led_index = -1);
    
    // Gamma correction utilities
    void buildAllGammaLUTs();
    void buildGammaLUT(CornerGamma& corner_gamma);
    cv::Vec3b applyGammaCorrection(const cv::Vec3b& color, int led_index) const;
    
    // Calculate blended gamma based on distance from 4 corners
    struct BlendedGamma {
        double red;
        double green;
        double blue;
    };
    BlendedGamma calculateBlendedGamma(int led_index) const;
    
    bool enable_parallel_;
    bool masks_precomputed_;
    std::string method_;  // "mean" or "dominant"
    std::vector<cv::Mat> cached_masks_;
    std::vector<cv::Rect> cached_bboxes_;
    
    // Gamma correction settings
    bool gamma_enabled_;
    LEDCounts led_counts_;
    
    // 8-point gamma settings with LUTs (4 corners + 4 edge centers)
    CornerGamma corner_gamma_top_left_;
    CornerGamma corner_gamma_top_center_;
    CornerGamma corner_gamma_top_right_;
    CornerGamma corner_gamma_right_center_;
    CornerGamma corner_gamma_bottom_right_;
    CornerGamma corner_gamma_bottom_center_;
    CornerGamma corner_gamma_bottom_left_;
    CornerGamma corner_gamma_left_center_;
};

} // namespace TVLED

