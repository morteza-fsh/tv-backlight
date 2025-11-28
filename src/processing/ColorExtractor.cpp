#include "processing/ColorExtractor.h"
#include "utils/Logger.h"
#include "utils/PerformanceTimer.h"
#include <algorithm>

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
                colors[idx] = extractSingleColorWithMask(frame, cached_masks_[idx], cached_bboxes_[idx]);
            }
        } else {
            for (size_t idx = 0; idx < polygons.size(); idx++) {
                colors[idx] = extractSingleColorWithMask(frame, cached_masks_[idx], cached_bboxes_[idx]);
            }
        }
        #else
        for (size_t idx = 0; idx < polygons.size(); idx++) {
            colors[idx] = extractSingleColorWithMask(frame, cached_masks_[idx], cached_bboxes_[idx]);
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
                colors[idx] = extractSingleColor(frame, polygons[idx], bboxes[idx]);
            }
        } else {
            for (size_t idx = 0; idx < polygons.size(); idx++) {
                colors[idx] = extractSingleColor(frame, polygons[idx], bboxes[idx]);
            }
        }
        #else
        for (size_t idx = 0; idx < polygons.size(); idx++) {
            colors[idx] = extractSingleColor(frame, polygons[idx], bboxes[idx]);
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
                                                    const cv::Rect& bbox) {
    if (bbox.width <= 0 || bbox.height <= 0 || mask.empty()) {
        return cv::Vec3b(0, 0, 0);
    }
    
    // Route to appropriate extraction method
    if (method_ == "dominant") {
        return extractDominantColor(frame, mask, bbox);
    } else {
        return extractMeanColor(frame, mask, bbox);
    }
}

cv::Vec3b ColorExtractor::extractMeanColor(const cv::Mat& frame,
                                           const cv::Mat& mask,
                                           const cv::Rect& bbox) {
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
        // Convert BGR to RGB and return
        uchar r = static_cast<uchar>(sum_r / pixel_count);
        uchar g = static_cast<uchar>(sum_g / pixel_count);
        uchar b = static_cast<uchar>(sum_b / pixel_count);
        return cv::Vec3b(r, g, b);  // Return as RGB
    }
    
    return cv::Vec3b(0, 0, 0);
}

cv::Vec3b ColorExtractor::extractDominantColor(const cv::Mat& frame,
                                               const cv::Mat& mask,
                                               const cv::Rect& bbox) {
    if (bbox.width <= 0 || bbox.height <= 0 || mask.empty()) {
        return cv::Vec3b(0, 0, 0);
    }
    
    // Collect all masked pixels
    std::vector<cv::Vec3b> pixels;
    pixels.reserve(bbox.width * bbox.height);
    
    for (int y = 0; y < bbox.height; y++) {
        const uchar* mask_row = mask.ptr<uchar>(y);
        const cv::Vec3b* img_row = frame.ptr<cv::Vec3b>(bbox.y + y) + bbox.x;
        
        for (int x = 0; x < bbox.width; x++) {
            if (mask_row[x]) {
                pixels.push_back(img_row[x]);
            }
        }
    }
    
    if (pixels.empty()) {
        return cv::Vec3b(0, 0, 0);
    }
    
    // For small regions, just use mean (k-means not effective)
    if (pixels.size() < 10) {
        uint32_t sum_b = 0, sum_g = 0, sum_r = 0;
        for (const auto& pixel : pixels) {
            sum_b += pixel[0];
            sum_g += pixel[1];
            sum_r += pixel[2];
        }
        uchar r = static_cast<uchar>(sum_r / pixels.size());
        uchar g = static_cast<uchar>(sum_g / pixels.size());
        uchar b = static_cast<uchar>(sum_b / pixels.size());
        return cv::Vec3b(r, g, b);  // Return as RGB
    }
    
    // Use k-means clustering to find dominant color
    // Convert pixels to float for k-means
    cv::Mat data(pixels.size(), 1, CV_32FC3);
    for (size_t i = 0; i < pixels.size(); i++) {
        data.at<cv::Vec3f>(i) = cv::Vec3f(
            static_cast<float>(pixels[i][0]),
            static_cast<float>(pixels[i][1]),
            static_cast<float>(pixels[i][2])
        );
    }
    
    // Run k-means with k=3 clusters, find the largest cluster
    int k = std::min(3, static_cast<int>(pixels.size()));
    cv::Mat labels, centers;
    cv::TermCriteria criteria(cv::TermCriteria::EPS + cv::TermCriteria::MAX_ITER, 10, 1.0);
    
    cv::kmeans(data, k, labels, criteria, 3, cv::KMEANS_PP_CENTERS, centers);
    
    // Find the cluster with the most pixels
    std::vector<int> cluster_counts(k, 0);
    for (int i = 0; i < labels.rows; i++) {
        cluster_counts[labels.at<int>(i)]++;
    }
    
    int dominant_cluster = 0;
    int max_count = cluster_counts[0];
    for (int i = 1; i < k; i++) {
        if (cluster_counts[i] > max_count) {
            max_count = cluster_counts[i];
            dominant_cluster = i;
        }
    }
    
    // Get the center color of the dominant cluster
    cv::Vec3f center = centers.at<cv::Vec3f>(dominant_cluster);
    
    // Convert BGR to RGB and return
    uchar r = static_cast<uchar>(center[2]);
    uchar g = static_cast<uchar>(center[1]);
    uchar b = static_cast<uchar>(center[0]);
    return cv::Vec3b(r, g, b);  // Return as RGB
}

cv::Vec3b ColorExtractor::extractSingleColor(const cv::Mat& frame,
                                             const std::vector<cv::Point>& polygon,
                                             const cv::Rect& bbox) {
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
    return extractSingleColorWithMask(frame, mask, bbox);
}

} // namespace TVLED

