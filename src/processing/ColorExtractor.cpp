#include "processing/ColorExtractor.h"
#include "processing/CoonsPatching.h"
#include "utils/Logger.h"
#include "utils/PerformanceTimer.h"
#include <algorithm>

#ifdef _OPENMP
#include <omp.h>
#endif

namespace TVLED {

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
    
    // Pre-compute bounding boxes
    std::vector<cv::Rect> bboxes(polygons.size());
    for (size_t i = 0; i < polygons.size(); i++) {
        bboxes[i] = cv::boundingRect(polygons[i]);
        bboxes[i] &= cv::Rect(0, 0, frame.cols, frame.rows);
    }
    
    // Parallel color extraction with OpenMP
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
    
    timer.stop();
    
    std::stringstream ss;
    ss << "Extracted " << colors.size() << " colors in " 
       << timer.elapsedMilliseconds() << " ms";
    LOG_DEBUG(ss.str());
    
    return colors;
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
    
    // Fast scanline sum using pointer arithmetic
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

std::vector<cv::Vec3b> ColorExtractor::extractEdgeSliceColors(
    const cv::Mat& frame,
    const CoonsPatching& coons,
    int horizontal_slices,
    int vertical_slices,
    float horizontal_coverage_percent,
    float vertical_coverage_percent,
    int polygon_samples) {
    
    PerformanceTimer timer("Edge slice color extraction", false);
    
    std::vector<std::vector<cv::Point>> polygons;
    polygons.reserve(2 * horizontal_slices + 2 * vertical_slices);
    
    // Calculate coverage in [0, 1] range
    float h_coverage = horizontal_coverage_percent / 100.0f;
    float v_coverage = vertical_coverage_percent / 100.0f;
    
    // Generate LEFT edge polygons (vertical slices) - reversed order (bottom to top)
    // These span vertically (v direction) and cover left v_coverage of width
    // Full height coverage (includes corner overlap with top/bottom)
    for (int i = vertical_slices - 1; i >= 0; i--) {
        double u0 = 0.0;
        double u1 = v_coverage;
        double v0 = static_cast<double>(i) / vertical_slices;
        double v1 = static_cast<double>(i + 1) / vertical_slices;
        
        polygons.push_back(coons.buildCellPolygon(u0, u1, v0, v1, polygon_samples));
    }
    
    // Generate TOP edge polygons (horizontal slices) - left to right
    // These span horizontally (u direction) but only cover top h_coverage of height
    for (int i = 0; i < horizontal_slices; i++) {
        double u0 = static_cast<double>(i) / horizontal_slices;
        double u1 = static_cast<double>(i + 1) / horizontal_slices;
        double v0 = 0.0;
        double v1 = h_coverage;
        
        polygons.push_back(coons.buildCellPolygon(u0, u1, v0, v1, polygon_samples));
    }
    
    // Generate RIGHT edge polygons (vertical slices) - top to bottom
    // These span vertically and cover right v_coverage of width
    // Full height coverage (includes corner overlap with top/bottom)
    for (int i = 0; i < vertical_slices; i++) {
        double u0 = 1.0 - v_coverage;
        double u1 = 1.0;
        double v0 = static_cast<double>(i) / vertical_slices;
        double v1 = static_cast<double>(i + 1) / vertical_slices;
        
        polygons.push_back(coons.buildCellPolygon(u0, u1, v0, v1, polygon_samples));
    }
    
    // Generate BOTTOM edge polygons (horizontal slices) - reversed order (right to left)
    // These span horizontally but only cover bottom h_coverage of height
    for (int i = horizontal_slices - 1; i >= 0; i--) {
        double u0 = static_cast<double>(i) / horizontal_slices;
        double u1 = static_cast<double>(i + 1) / horizontal_slices;
        double v0 = 1.0 - h_coverage;
        double v1 = 1.0;
        
        polygons.push_back(coons.buildCellPolygon(u0, u1, v0, v1, polygon_samples));
    }
    
    timer.stop();
    LOG_DEBUG("Generated " + std::to_string(polygons.size()) + " edge slice polygons in " +
              std::to_string(timer.elapsedMilliseconds()) + " ms");
    
    // Extract colors from the generated polygons
    return extractColors(frame, polygons);
}

} // namespace TVLED

