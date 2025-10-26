#include "processing/CoonsPatching.h"
#include "utils/Logger.h"
#include <algorithm>
#include <cmath>

namespace TVLED {

// PolylineCache implementation
PolylineCache::PolylineCache(const std::vector<cv::Point2f>& poly) 
    : poly_ptr_(&poly), total_length_(0.0) {
    cumulative_lengths_.reserve(poly.size());
    cumulative_lengths_.push_back(0.0);
    
    for (size_t i = 1; i < poly.size(); i++) {
        double dist = cv::norm(poly[i] - poly[i-1]);
        total_length_ += dist;
        cumulative_lengths_.push_back(total_length_);
    }
}

cv::Point2f PolylineCache::interp(double t) const {
    double d = std::max(0.0, std::min(1.0, t)) * total_length_;
    
    // Binary search for the segment (faster than linear)
    size_t i = 0;
    if (cumulative_lengths_.size() > 2) {
        size_t left = 0, right = cumulative_lengths_.size() - 1;
        while (left < right - 1) {
            size_t mid = (left + right) / 2;
            if (d <= cumulative_lengths_[mid]) {
                right = mid;
            } else {
                left = mid;
            }
        }
        i = left;
    }
    i = std::min(i, poly_ptr_->size() - 2);
    
    double span = cumulative_lengths_[i + 1] - cumulative_lengths_[i];
    double w = (span == 0) ? 0 : (d - cumulative_lengths_[i]) / span;
    
    const auto& poly = *poly_ptr_;
    return (1 - w) * poly[i] + w * poly[i + 1];
}

// CoonsPatching implementation
bool CoonsPatching::initialize(const std::vector<cv::Point2f>& top,
                               const std::vector<cv::Point2f>& right,
                               const std::vector<cv::Point2f>& bottom,
                               const std::vector<cv::Point2f>& left,
                               int imageWidth, int imageHeight) {
    if (top.empty() || right.empty() || bottom.empty() || left.empty()) {
        LOG_ERROR("Cannot initialize CoonsPatching with empty boundary curves");
        return false;
    }
    
    top_pts_ = top;
    right_pts_ = right;
    bottom_pts_ = bottom;
    left_pts_ = left;
    W_ = imageWidth;
    H_ = imageHeight;
    
    // Set corner points
    P00_ = top_pts_[0];                    // TL
    P10_ = top_pts_.back();                // TR
    P11_ = bottom_pts_.back();             // BR
    P01_ = bottom_pts_[0];                 // BL
    
    // Initialize caches for fast interpolation
    top_cache_ = std::make_unique<PolylineCache>(top_pts_);
    bottom_cache_ = std::make_unique<PolylineCache>(bottom_pts_);
    left_cache_ = std::make_unique<PolylineCache>(left_pts_);
    right_cache_ = std::make_unique<PolylineCache>(right_pts_);
    
    initialized_ = true;
    LOG_INFO("CoonsPatching initialized successfully");
    
    return true;
}

cv::Point2f CoonsPatching::C_top(double u) const {
    return top_cache_->interp(u);
}

cv::Point2f CoonsPatching::C_bottom(double u) const {
    return bottom_cache_->interp(u);
}

cv::Point2f CoonsPatching::D_left(double v) const {
    return left_cache_->interp(v);
}

cv::Point2f CoonsPatching::D_right(double v) const {
    return right_cache_->interp(v);
}

cv::Point2f CoonsPatching::interpolate(double u, double v) const {
    cv::Point2f c0 = C_top(u);
    cv::Point2f c1 = C_bottom(u);
    cv::Point2f d0 = D_left(v);
    cv::Point2f d1 = D_right(v);
    cv::Point2f B = (1-u)*(1-v)*P00_ + u*(1-v)*P10_ + u*v*P11_ + (1-u)*v*P01_;
    return (1-v)*c0 + v*c1 + (1-u)*d0 + u*d1 - B;
}

std::vector<cv::Point> CoonsPatching::buildCellPolygon(double u0, double u1,
                                                        double v0, double v1,
                                                        int samples) const {
    std::vector<cv::Point> poly;
    poly.reserve(samples * 4);
    
    double du = (u1 - u0) / (samples - 1);
    double dv = (v1 - v0) / (samples - 1);
    
    // Top edge: u from u0->u1 at v=v0
    for (int i = 0; i < samples; i++) {
        double u = u0 + du * i;
        cv::Point2f pt = interpolate(u, v0);
        poly.push_back(cv::Point(
            static_cast<int>(std::clamp(pt.x, 0.0f, static_cast<float>(W_-1))),
            static_cast<int>(std::clamp(pt.y, 0.0f, static_cast<float>(H_-1)))
        ));
    }
    
    // Right edge: v from v0->v1 at u=u1
    for (int i = 1; i < samples; i++) {
        double v = v0 + dv * i;
        cv::Point2f pt = interpolate(u1, v);
        poly.push_back(cv::Point(
            static_cast<int>(std::clamp(pt.x, 0.0f, static_cast<float>(W_-1))),
            static_cast<int>(std::clamp(pt.y, 0.0f, static_cast<float>(H_-1)))
        ));
    }
    
    // Bottom edge: u from u1->u0 at v=v1 (reverse)
    for (int i = 1; i < samples; i++) {
        double u = u1 - du * i;
        cv::Point2f pt = interpolate(u, v1);
        poly.push_back(cv::Point(
            static_cast<int>(std::clamp(pt.x, 0.0f, static_cast<float>(W_-1))),
            static_cast<int>(std::clamp(pt.y, 0.0f, static_cast<float>(H_-1)))
        ));
    }
    
    // Left edge: v from v1->v0 at u=u0 (reverse)
    for (int i = 1; i < samples; i++) {
        double v = v1 - dv * i;
        cv::Point2f pt = interpolate(u0, v);
        poly.push_back(cv::Point(
            static_cast<int>(std::clamp(pt.x, 0.0f, static_cast<float>(W_-1))),
            static_cast<int>(std::clamp(pt.y, 0.0f, static_cast<float>(H_-1)))
        ));
    }
    
    return poly;
}

cv::Point2f CoonsPatching::getCorner(int index) const {
    switch(index) {
        case 0: return P00_;  // TL
        case 1: return P10_;  // TR
        case 2: return P11_;  // BR
        case 3: return P01_;  // BL
        default: return cv::Point2f(0, 0);
    }
}

} // namespace TVLED

