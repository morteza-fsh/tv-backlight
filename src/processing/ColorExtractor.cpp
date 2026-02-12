#include "processing/ColorExtractor.h"
#include "utils/Logger.h"
#include "utils/PerformanceTimer.h"
#include <algorithm>
#include <cmath>

#ifdef _OPENMP
#include <omp.h>
#endif

// NEON SIMD support for ARM processors
#if defined(__ARM_NEON) || defined(__ARM_NEON__)
#include <arm_neon.h>
#define USE_NEON_SIMD 1
#endif

namespace TVLED {

#ifdef USE_NEON_SIMD
// NEON SIMD optimized color accumulation
// Processes 16 pixels at a time using NEON intrinsics
inline void accumulateColorsNEON(const cv::Vec3b* img_row, const uchar* mask_row, 
                                  int width, uint32_t& sum_b, uint32_t& sum_g, 
                                  uint32_t& sum_r, int& pixel_count) {
    int x = 0;
    
    // NEON accumulators (32-bit to prevent overflow)
    uint32x4_t acc_b = vdupq_n_u32(0);
    uint32x4_t acc_g = vdupq_n_u32(0);
    uint32x4_t acc_r = vdupq_n_u32(0);
    uint32x4_t acc_count = vdupq_n_u32(0);
    
    // Process 16 pixels at a time (NEON optimal width)
    for (; x + 15 < width; x += 16) {
        // Load 16 mask bytes
        uint8x16_t mask_vec = vld1q_u8(mask_row + x);
        
        // Check if all masks are zero (early skip optimization)
        uint64x2_t mask_u64 = vreinterpretq_u64_u8(mask_vec);
        uint64_t mask_check = vgetq_lane_u64(mask_u64, 0) | vgetq_lane_u64(mask_u64, 1);
        if (mask_check == 0) {
            continue; // Skip if all masks are zero
        }
        
        // Process in two chunks of 8 pixels each for better register usage
        for (int chunk = 0; chunk < 2; chunk++) {
            int offset = x + chunk * 8;
            
            // Load 8 BGR pixels (24 bytes)
            // OpenCV stores as BGR, so we need to deinterleave
            uint8x8x3_t pixels = vld3_u8(reinterpret_cast<const uint8_t*>(img_row + offset));
            
            // Load 8 mask bytes
            uint8x8_t mask_chunk = vld1_u8(mask_row + offset);
            
            // Convert mask to 0 or 0xFF for each pixel
            uint8x8_t mask_bool = vcgt_u8(mask_chunk, vdup_n_u8(0));
            
            // Widen mask to 16-bit for masking operations
            uint16x8_t mask_wide = vmovl_u8(mask_bool);
            
            // Widen pixel values to 16-bit
            uint16x8_t b_wide = vmovl_u8(pixels.val[0]);
            uint16x8_t g_wide = vmovl_u8(pixels.val[1]);
            uint16x8_t r_wide = vmovl_u8(pixels.val[2]);
            
            // Apply mask (element-wise AND)
            b_wide = vandq_u16(b_wide, mask_wide);
            g_wide = vandq_u16(g_wide, mask_wide);
            r_wide = vandq_u16(r_wide, mask_wide);
            
            // Accumulate into 32-bit accumulators (two halves)
            // Low 4 elements
            uint16x4_t b_low = vget_low_u16(b_wide);
            uint16x4_t g_low = vget_low_u16(g_wide);
            uint16x4_t r_low = vget_low_u16(r_wide);
            uint16x4_t mask_low = vget_low_u16(mask_wide);
            
            acc_b = vaddq_u32(acc_b, vmovl_u16(b_low));
            acc_g = vaddq_u32(acc_g, vmovl_u16(g_low));
            acc_r = vaddq_u32(acc_r, vmovl_u16(r_low));
            
            // Convert mask to count: mask_wide is 0xFFFF for valid pixels, 0 otherwise
            // Right shift by 15 gives us 1 for valid, 0 for invalid
            uint32x4_t mask_low_32 = vmovl_u16(mask_low);
            uint32x4_t count_low = vshrq_n_u32(mask_low_32, 15);
            // But we need to account for the fact that it might be 0xFFFF, not just any value
            // Actually, vandq gives us the pixel value if mask is 0xFFFF, 0 otherwise
            // So let's just check if the mask is non-zero
            uint32x4_t mask_low_bool = vcgtq_u32(mask_low_32, vdupq_n_u32(0));
            count_low = vandq_u32(mask_low_bool, vdupq_n_u32(1));
            acc_count = vaddq_u32(acc_count, count_low);
            
            // High 4 elements
            uint16x4_t b_high = vget_high_u16(b_wide);
            uint16x4_t g_high = vget_high_u16(g_wide);
            uint16x4_t r_high = vget_high_u16(r_wide);
            uint16x4_t mask_high = vget_high_u16(mask_wide);
            
            acc_b = vaddq_u32(acc_b, vmovl_u16(b_high));
            acc_g = vaddq_u32(acc_g, vmovl_u16(g_high));
            acc_r = vaddq_u32(acc_r, vmovl_u16(r_high));
            
            uint32x4_t mask_high_32 = vmovl_u16(mask_high);
            uint32x4_t mask_high_bool = vcgtq_u32(mask_high_32, vdupq_n_u32(0));
            uint32x4_t count_high = vandq_u32(mask_high_bool, vdupq_n_u32(1));
            acc_count = vaddq_u32(acc_count, count_high);
        }
    }
    
    // Reduce NEON accumulators to scalars
    // Using pairwise addition for efficient reduction
    uint32x2_t acc_b_pair = vadd_u32(vget_low_u32(acc_b), vget_high_u32(acc_b));
    uint32x2_t acc_g_pair = vadd_u32(vget_low_u32(acc_g), vget_high_u32(acc_g));
    uint32x2_t acc_r_pair = vadd_u32(vget_low_u32(acc_r), vget_high_u32(acc_r));
    uint32x2_t acc_count_pair = vadd_u32(vget_low_u32(acc_count), vget_high_u32(acc_count));
    
    sum_b += vget_lane_u32(vpadd_u32(acc_b_pair, acc_b_pair), 0);
    sum_g += vget_lane_u32(vpadd_u32(acc_g_pair, acc_g_pair), 0);
    sum_r += vget_lane_u32(vpadd_u32(acc_r_pair, acc_r_pair), 0);
    pixel_count += vget_lane_u32(vpadd_u32(acc_count_pair, acc_count_pair), 0);
    
    // Process remaining pixels with scalar code
    for (; x < width; x++) {
        if (mask_row[x]) {
            const cv::Vec3b& pixel = img_row[x];
            sum_b += pixel[0];
            sum_g += pixel[1];
            sum_r += pixel[2];
            pixel_count++;
        }
    }
}
#endif

void ColorExtractor::precomputeMasks(const std::vector<std::vector<cv::Point>>& polygons,
                                     int frame_width, int frame_height) {
    cached_masks_.clear();
    cached_bboxes_.clear();
    cached_masks_.reserve(polygons.size());
    cached_bboxes_.reserve(polygons.size());
    
#ifdef USE_NEON_SIMD
    LOG_INFO("Pre-computing " + std::to_string(polygons.size()) + " masks with NEON SIMD enabled...");
#else
    LOG_INFO("Pre-computing " + std::to_string(polygons.size()) + " masks...");
#endif
    PerformanceTimer timer("Mask pre-computation", false);
    
    for (const auto& polygon : polygons) {
        cv::Rect bbox = cv::boundingRect(polygon);
        bbox &= cv::Rect(0, 0, frame_width, frame_height);
        
        if (bbox.width <= 0 || bbox.height <= 0) {
            // Empty mask for invalid bounding box
            cached_masks_.push_back(cv::Mat());
            cached_bboxes_.push_back(bbox);
            continue;
        }
        
        // Create mask for this polygon
        cv::Mat mask = cv::Mat::zeros(bbox.height, bbox.width, CV_8UC1);
        std::vector<cv::Point> poly_relative;
        poly_relative.reserve(polygon.size());
        
        for (const auto& pt : polygon) {
            poly_relative.push_back(cv::Point(pt.x - bbox.x, pt.y - bbox.y));
        }
        
        cv::fillPoly(mask, std::vector<std::vector<cv::Point>>{poly_relative}, cv::Scalar(255));
        
        cached_masks_.push_back(mask);
        cached_bboxes_.push_back(bbox);
    }
    
    timer.stop();
    masks_precomputed_ = true;
    LOG_INFO("Mask pre-computation completed in " + 
             std::to_string(timer.elapsedMilliseconds()) + " ms");
}

std::vector<cv::Vec3b> ColorExtractor::extractColors(
    const cv::Mat& frame,
    const std::vector<std::vector<cv::Point>>& polygons) {
    
    std::vector<cv::Vec3b> colors;
    colors.resize(polygons.size());
    
    if (polygons.empty()) {
        LOG_WARN("No polygons provided for color extraction");
        return colors;
    }
    
    PerformanceTimer timer("Color extraction", false);
    
    // Use pre-computed masks if available, otherwise fall back to dynamic creation
    if (masks_precomputed_ && cached_masks_.size() == polygons.size()) {
        // Fast path: use pre-computed masks
        #ifdef _OPENMP
        if (enable_parallel_) {
            #pragma omp parallel for schedule(dynamic, 4)
            for (int idx = 0; idx < static_cast<int>(polygons.size()); idx++) {
                colors[idx] = extractSingleColorWithMask(frame, cached_masks_[idx], cached_bboxes_[idx], idx);
            }
        } else {
            for (size_t idx = 0; idx < polygons.size(); idx++) {
                colors[idx] = extractSingleColorWithMask(frame, cached_masks_[idx], cached_bboxes_[idx], idx);
            }
        }
        #else
        for (size_t idx = 0; idx < polygons.size(); idx++) {
            colors[idx] = extractSingleColorWithMask(frame, cached_masks_[idx], cached_bboxes_[idx], idx);
        }
        #endif
    } else {
        // Fallback: compute masks dynamically (original behavior)
        std::vector<cv::Rect> bboxes(polygons.size());
        for (size_t i = 0; i < polygons.size(); i++) {
            bboxes[i] = cv::boundingRect(polygons[i]);
            bboxes[i] &= cv::Rect(0, 0, frame.cols, frame.rows);
        }
        
        #ifdef _OPENMP
        if (enable_parallel_) {
            #pragma omp parallel for schedule(dynamic, 4)
            for (int idx = 0; idx < static_cast<int>(polygons.size()); idx++) {
                colors[idx] = extractSingleColor(frame, polygons[idx], bboxes[idx], idx);
            }
        } else {
            for (size_t idx = 0; idx < polygons.size(); idx++) {
                colors[idx] = extractSingleColor(frame, polygons[idx], bboxes[idx], idx);
            }
        }
        #else
        for (size_t idx = 0; idx < polygons.size(); idx++) {
            colors[idx] = extractSingleColor(frame, polygons[idx], bboxes[idx], idx);
        }
        #endif
    }
    
    timer.stop();
    
    std::stringstream ss;
    ss << "Extracted " << colors.size() << " colors in " 
       << timer.elapsedMilliseconds() << " ms";
    LOG_DEBUG(ss.str());
    
    return colors;
}

cv::Vec3b ColorExtractor::extractSingleColorWithMask(const cv::Mat& frame,
                                                    const cv::Mat& mask,
                                                    const cv::Rect& bbox,
                                                    int led_index) {
    if (bbox.width <= 0 || bbox.height <= 0 || mask.empty()) {
        return cv::Vec3b(0, 0, 0);
    }
    
    // Route to appropriate extraction method
    if (method_ == "dominant") {
        return extractDominantColor(frame, mask, bbox, led_index);
    } else {
        return extractMeanColor(frame, mask, bbox, led_index);
    }
}

cv::Vec3b ColorExtractor::extractMeanColor(const cv::Mat& frame,
                                           const cv::Mat& mask,
                                           const cv::Rect& bbox,
                                           int led_index) {
    if (bbox.width <= 0 || bbox.height <= 0 || mask.empty()) {
        return cv::Vec3b(0, 0, 0);
    }
    
    // Color accumulators
    uint32_t sum_b = 0, sum_g = 0, sum_r = 0;
    int pixel_count = 0;
    
#ifdef USE_NEON_SIMD
    // NEON SIMD optimized path - processes 16 pixels at a time
    for (int y = 0; y < bbox.height; y++) {
        const uchar* mask_row = mask.ptr<uchar>(y);
        const cv::Vec3b* img_row = frame.ptr<cv::Vec3b>(bbox.y + y) + bbox.x;
        
        accumulateColorsNEON(img_row, mask_row, bbox.width, 
                            sum_b, sum_g, sum_r, pixel_count);
    }
#else
    // Scalar fallback for non-ARM platforms
    for (int y = 0; y < bbox.height; y++) {
        const uchar* mask_row = mask.ptr<uchar>(y);
        const cv::Vec3b* img_row = frame.ptr<cv::Vec3b>(bbox.y + y) + bbox.x;
        
        for (int x = 0; x < bbox.width; x++) {
            if (mask_row[x]) {
                const cv::Vec3b& pixel = img_row[x];
                sum_b += pixel[0];
                sum_g += pixel[1];
                sum_r += pixel[2];
                pixel_count++;
            }
        }
    }
#endif
    
    if (pixel_count > 0) {
        // Convert BGR to RGB
        uchar r = static_cast<uchar>(sum_r / pixel_count);
        uchar g = static_cast<uchar>(sum_g / pixel_count);
        uchar b = static_cast<uchar>(sum_b / pixel_count);
        cv::Vec3b color(r, g, b);
        
        // Apply gamma correction if enabled
        return applyGammaCorrection(color, led_index);
    }
    
    return cv::Vec3b(0, 0, 0);
}

cv::Vec3b ColorExtractor::extractDominantColor(const cv::Mat& frame,
                                               const cv::Mat& mask,
                                               const cv::Rect& bbox,
                                               int led_index) {
    if (bbox.width <= 0 || bbox.height <= 0 || mask.empty()) {
        return cv::Vec3b(0, 0, 0);
    }
    
    // Use histogram-based approach for performance
    // Quantize colors to 8 bins per channel (512 total bins)
    constexpr int bins_per_channel = 8;
    constexpr int bin_shift = 8 - 3;  // 2^3 = 8 bins
    constexpr int total_bins = bins_per_channel * bins_per_channel * bins_per_channel;
    
    // Count pixels in each quantized color bin
    std::vector<int> histogram(total_bins, 0);
    std::vector<uint32_t> sum_b(total_bins, 0);
    std::vector<uint32_t> sum_g(total_bins, 0);
    std::vector<uint32_t> sum_r(total_bins, 0);
    int total_pixels = 0;
    
    // Build histogram (scalar implementation - SIMD not beneficial for histogram updates)
    for (int y = 0; y < bbox.height; y++) {
        const uchar* mask_row = mask.ptr<uchar>(y);
        const cv::Vec3b* img_row = frame.ptr<cv::Vec3b>(bbox.y + y) + bbox.x;
        
        for (int x = 0; x < bbox.width; x++) {
            if (mask_row[x]) {
                const cv::Vec3b& pixel = img_row[x];
                
                // Quantize to bin indices
                int b_bin = pixel[0] >> bin_shift;
                int g_bin = pixel[1] >> bin_shift;
                int r_bin = pixel[2] >> bin_shift;
                
                // Compute linear bin index
                int bin_idx = (r_bin * bins_per_channel * bins_per_channel) + 
                             (g_bin * bins_per_channel) + b_bin;
                
                histogram[bin_idx]++;
                sum_b[bin_idx] += pixel[0];
                sum_g[bin_idx] += pixel[1];
                sum_r[bin_idx] += pixel[2];
                total_pixels++;
            }
        }
    }
    
    if (total_pixels == 0) {
        return cv::Vec3b(0, 0, 0);
    }
    
    // Find the bin with the most pixels (dominant color)
    int max_bin = 0;
    int max_count = histogram[0];
    
#ifdef USE_NEON_SIMD
    // NEON optimization for finding maximum in histogram
    // Process 4 elements at a time
    int32x4_t max_count_vec = vdupq_n_s32(histogram[0]);
    const int32_t init_bins[4] = {0, 1, 2, 3};
    int32x4_t max_bin_vec = vld1q_s32(init_bins);
    int32x4_t idx_increment = vdupq_n_s32(4);
    int32x4_t current_idx = vld1q_s32(init_bins);
    
    for (int i = 0; i < total_bins - 3; i += 4) {
        int32x4_t hist_vec = vld1q_s32(reinterpret_cast<const int32_t*>(&histogram[i]));
        uint32x4_t cmp = vcgtq_s32(hist_vec, max_count_vec);
        
        // Update max values where comparison is true
        max_count_vec = vbslq_s32(cmp, hist_vec, max_count_vec);
        max_bin_vec = vbslq_s32(cmp, current_idx, max_bin_vec);
        
        current_idx = vaddq_s32(current_idx, idx_increment);
    }
    
    // Reduce NEON results to scalar
    int32_t counts[4], bins[4];
    vst1q_s32(counts, max_count_vec);
    vst1q_s32(bins, max_bin_vec);
    
    max_count = counts[0];
    max_bin = bins[0];
    for (int i = 1; i < 4; i++) {
        if (counts[i] > max_count) {
            max_count = counts[i];
            max_bin = bins[i];
        }
    }
    
    // Handle remaining elements
    for (int i = (total_bins & ~3); i < total_bins; i++) {
        if (histogram[i] > max_count) {
            max_count = histogram[i];
            max_bin = i;
        }
    }
#else
    // Scalar fallback
    for (int i = 1; i < total_bins; i++) {
        if (histogram[i] > max_count) {
            max_count = histogram[i];
            max_bin = i;
        }
    }
#endif
    
    // Calculate average color within the dominant bin
    if (max_count > 0) {
        uchar r = static_cast<uchar>(sum_r[max_bin] / max_count);
        uchar g = static_cast<uchar>(sum_g[max_bin] / max_count);
        uchar b = static_cast<uchar>(sum_b[max_bin] / max_count);
        cv::Vec3b color(r, g, b);
        
        // Apply gamma correction if enabled
        return applyGammaCorrection(color, led_index);
    }
    
    return cv::Vec3b(0, 0, 0);
}

cv::Vec3b ColorExtractor::extractSingleColor(const cv::Mat& frame,
                                             const std::vector<cv::Point>& polygon,
                                             const cv::Rect& bbox,
                                             int led_index) {
    if (bbox.width <= 0 || bbox.height <= 0) {
        return cv::Vec3b(0, 0, 0);
    }
    
    // Create minimal mask for scanline processing
    cv::Mat mask = cv::Mat::zeros(bbox.height, bbox.width, CV_8UC1);
    std::vector<cv::Point> poly_relative;
    poly_relative.reserve(polygon.size());
    
    for (const auto& pt : polygon) {
        poly_relative.push_back(cv::Point(pt.x - bbox.x, pt.y - bbox.y));
    }
    
    cv::fillPoly(mask, std::vector<std::vector<cv::Point>>{poly_relative}, cv::Scalar(255));
    
    // Use the optimized mask-based extraction
    return extractSingleColorWithMask(frame, mask, bbox, led_index);
}

void ColorExtractor::buildAllGammaLUTs() {
    buildGammaLUT(corner_gamma_top_left_);
    buildGammaLUT(corner_gamma_top_right_);
    buildGammaLUT(corner_gamma_bottom_left_);
    buildGammaLUT(corner_gamma_bottom_right_);
    
    LOG_DEBUG("All corner gamma correction LUTs built");
}

void ColorExtractor::buildGammaLUT(CornerGamma& corner_gamma) {
    // Pre-compute gamma correction lookup tables for fast processing
    corner_gamma.lut_red.resize(256);
    corner_gamma.lut_green.resize(256);
    corner_gamma.lut_blue.resize(256);
    
    for (int i = 0; i < 256; i++) {
        // Normalize to [0, 1] range
        double normalized = i / 255.0;
        
        // Apply gamma correction: output = input^(1/gamma)
        // This converts from linear light to display gamma
        double corrected_r = std::pow(normalized, 1.0 / corner_gamma.gamma_red);
        double corrected_g = std::pow(normalized, 1.0 / corner_gamma.gamma_green);
        double corrected_b = std::pow(normalized, 1.0 / corner_gamma.gamma_blue);
        
        // Scale back to [0, 255] and clamp
        corner_gamma.lut_red[i] = static_cast<uchar>(std::min(255.0, std::max(0.0, corrected_r * 255.0 + 0.5)));
        corner_gamma.lut_green[i] = static_cast<uchar>(std::min(255.0, std::max(0.0, corrected_g * 255.0 + 0.5)));
        corner_gamma.lut_blue[i] = static_cast<uchar>(std::min(255.0, std::max(0.0, corrected_b * 255.0 + 0.5)));
    }
}

ColorExtractor::BlendedGamma ColorExtractor::calculateBlendedGamma(int led_index) const {
    BlendedGamma result;
    
    // If led_index is invalid or layout not set, use top-left corner gamma as default
    if (led_index < 0 || led_layout_.getTotalLEDs() == 0) {
        result.red = corner_gamma_top_left_.gamma_red;
        result.green = corner_gamma_top_left_.gamma_green;
        result.blue = corner_gamma_top_left_.gamma_blue;
        return result;
    }
    
    // LED layout: [left_leds] -> [top_leds] -> [right_leds] -> [bottom_leds]
    // Starting from bottom-left corner, going counter-clockwise
    
    // Calculate distance from each of the 4 corners
    double dist_tl, dist_tr, dist_bl, dist_br;
    
    int current_idx = 0;
    
    // Left edge: indices [0, left)
    if (led_index < led_layout_.left) {
        int pos_on_edge = led_index;
        dist_tl = led_layout_.left - 1 - pos_on_edge;  // Distance to top-left corner
        dist_bl = pos_on_edge;  // Distance to bottom-left corner
        dist_tr = led_layout_.left + led_layout_.top - 1 - pos_on_edge;  // Far
        dist_br = pos_on_edge + led_layout_.bottom;  // Far
    }
    // Top edge: indices [left, left+top)
    else if (led_index < led_layout_.left + led_layout_.top) {
        int pos_on_edge = led_index - led_layout_.left;
        dist_tl = pos_on_edge;  // Distance to top-left corner
        dist_tr = led_layout_.top - 1 - pos_on_edge;  // Distance to top-right corner
        dist_bl = pos_on_edge + led_layout_.left;  // Far
        dist_br = led_layout_.top - 1 - pos_on_edge + led_layout_.right;  // Far
    }
    // Right edge: indices [left+top, left+top+right)
    else if (led_index < led_layout_.left + led_layout_.top + led_layout_.right) {
        int pos_on_edge = led_index - led_layout_.left - led_layout_.top;
        dist_tr = pos_on_edge;  // Distance to top-right corner
        dist_br = led_layout_.right - 1 - pos_on_edge;  // Distance to bottom-right corner
        dist_tl = pos_on_edge + led_layout_.top;  // Far
        dist_bl = led_layout_.right - 1 - pos_on_edge + led_layout_.bottom;  // Far
    }
    // Bottom edge: indices [left+top+right, left+top+right+bottom)
    else {
        int pos_on_edge = led_index - led_layout_.left - led_layout_.top - led_layout_.right;
        dist_br = pos_on_edge;  // Distance to bottom-right corner
        dist_bl = led_layout_.bottom - 1 - pos_on_edge;  // Distance to bottom-left corner
        dist_tr = pos_on_edge + led_layout_.right;  // Far
        dist_tl = led_layout_.bottom - 1 - pos_on_edge + led_layout_.left;  // Far
    }
    
    // Calculate weights using inverse distance weighting
    // Add small epsilon to avoid division by zero
    const double epsilon = 1.0;
    double weight_tl = 1.0 / (dist_tl + epsilon);
    double weight_tr = 1.0 / (dist_tr + epsilon);
    double weight_bl = 1.0 / (dist_bl + epsilon);
    double weight_br = 1.0 / (dist_br + epsilon);
    
    double total_weight = weight_tl + weight_tr + weight_bl + weight_br;
    
    // Normalize weights
    weight_tl /= total_weight;
    weight_tr /= total_weight;
    weight_bl /= total_weight;
    weight_br /= total_weight;
    
    // Blend gamma values from all 4 corners
    result.red = weight_tl * corner_gamma_top_left_.gamma_red +
                 weight_tr * corner_gamma_top_right_.gamma_red +
                 weight_bl * corner_gamma_bottom_left_.gamma_red +
                 weight_br * corner_gamma_bottom_right_.gamma_red;
    
    result.green = weight_tl * corner_gamma_top_left_.gamma_green +
                   weight_tr * corner_gamma_top_right_.gamma_green +
                   weight_bl * corner_gamma_bottom_left_.gamma_green +
                   weight_br * corner_gamma_bottom_right_.gamma_green;
    
    result.blue = weight_tl * corner_gamma_top_left_.gamma_blue +
                  weight_tr * corner_gamma_top_right_.gamma_blue +
                  weight_bl * corner_gamma_bottom_left_.gamma_blue +
                  weight_br * corner_gamma_bottom_right_.gamma_blue;
    
    return result;
}

cv::Vec3b ColorExtractor::applyGammaCorrection(const cv::Vec3b& color, int led_index) const {
    if (!gamma_enabled_) {
        return color;
    }
    
    // Get blended gamma values for this LED
    BlendedGamma gamma = calculateBlendedGamma(led_index);
    
    // Apply gamma correction using the blended gamma values
    // We need to compute on-the-fly for blended gammas
    double normalized_r = color[0] / 255.0;
    double normalized_g = color[1] / 255.0;
    double normalized_b = color[2] / 255.0;
    
    double corrected_r = std::pow(normalized_r, 1.0 / gamma.red);
    double corrected_g = std::pow(normalized_g, 1.0 / gamma.green);
    double corrected_b = std::pow(normalized_b, 1.0 / gamma.blue);
    
    // Scale back to [0, 255] and clamp
    uchar final_r = static_cast<uchar>(std::min(255.0, std::max(0.0, corrected_r * 255.0 + 0.5)));
    uchar final_g = static_cast<uchar>(std::min(255.0, std::max(0.0, corrected_g * 255.0 + 0.5)));
    uchar final_b = static_cast<uchar>(std::min(255.0, std::max(0.0, corrected_b * 255.0 + 0.5)));
    
    return cv::Vec3b(final_r, final_g, final_b);
}

} // namespace TVLED

