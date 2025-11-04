#include "processing/ColorExtractor.h"
#include "utils/Logger.h"
#include "utils/PerformanceTimer.h"
#include <algorithm>

#ifdef _OPENMP
#include <omp.h>
#endif

namespace TVLED {

void ColorExtractor::precomputeMasks(const std::vector<std::vector<cv::Point>>& polygons,
                                     int frame_width, int frame_height) {
    cached_masks_.clear();
    cached_bboxes_.clear();
    cached_masks_.reserve(polygons.size());
    cached_bboxes_.reserve(polygons.size());
    
    LOG_INFO("Pre-computing " + std::to_string(polygons.size()) + " masks...");
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
    
    // Fast scanline sum using pointer arithmetic (no mask creation needed)
    long long sum_b = 0, sum_g = 0, sum_r = 0;
    int pixel_count = 0;
    
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
    
    if (pixel_count > 0) {
        // Convert BGR to RGB and return
        uchar r = static_cast<uchar>(sum_r / pixel_count);
        uchar g = static_cast<uchar>(sum_g / pixel_count);
        uchar b = static_cast<uchar>(sum_b / pixel_count);
        return cv::Vec3b(r, g, b);  // Return as RGB
    }
    
    return cv::Vec3b(0, 0, 0);
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

