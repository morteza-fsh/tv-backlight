// Bottleneck Analysis Tool
// This program helps identify where time is actually spent in the processing pipeline

#include <iostream>
#include <chrono>
#include <opencv2/opencv.hpp>
#include <arm_neon.h>

using namespace std;
using namespace std::chrono;

// Simulate different processing patterns
void test_memory_bandwidth(int width, int height, int iterations) {
    cout << "\n=== Testing Memory Bandwidth ===" << endl;
    
    // Create test image
    cv::Mat img(height, width, CV_8UC3);
    cv::Mat mask(height, width, CV_8UC1, cv::Scalar(255));
    
    // Test 1: Scalar pixel access
    {
        auto start = high_resolution_clock::now();
        uint64_t sum = 0;
        
        for (int iter = 0; iter < iterations; iter++) {
            for (int y = 0; y < height; y++) {
                const uchar* mask_row = mask.ptr<uchar>(y);
                const cv::Vec3b* img_row = img.ptr<cv::Vec3b>(y);
                
                for (int x = 0; x < width; x++) {
                    if (mask_row[x]) {
                        sum += img_row[x][0] + img_row[x][1] + img_row[x][2];
                    }
                }
            }
        }
        
        auto end = high_resolution_clock::now();
        auto duration_us = duration_cast<microseconds>(end - start).count();
        double mpixels = (width * height * iterations) / 1000000.0;
        double mpixels_per_sec = mpixels / (duration_us / 1000000.0);
        
        cout << "Scalar access: " << duration_us / 1000.0 << " ms" << endl;
        cout << "  Throughput: " << mpixels_per_sec << " Mpixels/sec" << endl;
        cout << "  Time per pixel: " << (duration_us * 1000.0) / (width * height * iterations) << " ns" << endl;
    }
    
    // Test 2: NEON access (simplified)
    {
        auto start = high_resolution_clock::now();
        uint64_t sum = 0;
        
        for (int iter = 0; iter < iterations; iter++) {
            for (int y = 0; y < height; y++) {
                const uchar* mask_row = mask.ptr<uchar>(y);
                const cv::Vec3b* img_row = img.ptr<cv::Vec3b>(y);
                int x = 0;
                
                // NEON processing
                uint32x4_t acc_sum = vdupq_n_u32(0);
                
                for (; x + 15 < width; x += 16) {
                    // Load 16 mask bytes
                    uint8x16_t mask_vec = vld1q_u8(mask_row + x);
                    
                    // Process 8 pixels at a time
                    for (int chunk = 0; chunk < 2; chunk++) {
                        int offset = x + chunk * 8;
                        uint8x8x3_t pixels = vld3_u8(reinterpret_cast<const uint8_t*>(img_row + offset));
                        uint16x8_t b_wide = vmovl_u8(pixels.val[0]);
                        uint16x8_t g_wide = vmovl_u8(pixels.val[1]);
                        uint16x8_t r_wide = vmovl_u8(pixels.val[2]);
                        
                        uint16x8_t sum_wide = vaddq_u16(vaddq_u16(b_wide, g_wide), r_wide);
                        acc_sum = vaddq_u32(acc_sum, vmovl_u16(vget_low_u16(sum_wide)));
                        acc_sum = vaddq_u32(acc_sum, vmovl_u16(vget_high_u16(sum_wide)));
                    }
                }
                
                // Scalar remainder
                for (; x < width; x++) {
                    if (mask_row[x]) {
                        sum += img_row[x][0] + img_row[x][1] + img_row[x][2];
                    }
                }
                
                // Reduce accumulator
                uint32x2_t pair = vadd_u32(vget_low_u32(acc_sum), vget_high_u32(acc_sum));
                sum += vget_lane_u32(vpadd_u32(pair, pair), 0);
            }
        }
        
        auto end = high_resolution_clock::now();
        auto duration_us = duration_cast<microseconds>(end - start).count();
        double mpixels = (width * height * iterations) / 1000000.0;
        double mpixels_per_sec = mpixels / (duration_us / 1000000.0);
        
        cout << "NEON access: " << duration_us / 1000.0 << " ms" << endl;
        cout << "  Throughput: " << mpixels_per_sec << " Mpixels/sec" << endl;
        cout << "  Time per pixel: " << (duration_us * 1000.0) / (width * height * iterations) << " ns" << endl;
    }
}

void test_region_sizes(const vector<int>& sizes) {
    cout << "\n=== Testing Different Region Sizes ===" << endl;
    
    for (int size : sizes) {
        cv::Mat img(size, size, CV_8UC3);
        cv::Mat mask(size, size, CV_8UC1, cv::Scalar(255));
        
        int iterations = std::max(1, 10000 / (size * size)); // Adjust iterations based on size
        
        auto start = high_resolution_clock::now();
        uint64_t sum = 0;
        
        for (int iter = 0; iter < iterations; iter++) {
            for (int y = 0; y < size; y++) {
                const uchar* mask_row = mask.ptr<uchar>(y);
                const cv::Vec3b* img_row = img.ptr<cv::Vec3b>(y);
                int x = 0;
                
                uint32x4_t acc_sum = vdupq_n_u32(0);
                for (; x + 15 < size; x += 16) {
                    uint8x16_t mask_vec = vld1q_u8(mask_row + x);
                    for (int chunk = 0; chunk < 2; chunk++) {
                        int offset = x + chunk * 8;
                        uint8x8x3_t pixels = vld3_u8(reinterpret_cast<const uint8_t*>(img_row + offset));
                        uint16x8_t sum_wide = vaddq_u16(vaddq_u16(vmovl_u8(pixels.val[0]), 
                                                                   vmovl_u8(pixels.val[1])), 
                                                        vmovl_u8(pixels.val[2]));
                        acc_sum = vaddq_u32(acc_sum, vmovl_u16(vget_low_u16(sum_wide)));
                        acc_sum = vaddq_u32(acc_sum, vmovl_u16(vget_high_u16(sum_wide)));
                    }
                }
                
                for (; x < size; x++) {
                    if (mask_row[x]) {
                        sum += img_row[x][0] + img_row[x][1] + img_row[x][2];
                    }
                }
                
                uint32x2_t pair = vadd_u32(vget_low_u32(acc_sum), vget_high_u32(acc_sum));
                sum += vget_lane_u32(vpadd_u32(pair, pair), 0);
            }
        }
        
        auto end = high_resolution_clock::now();
        auto duration_us = duration_cast<microseconds>(end - start).count();
        double time_per_region = duration_us / (double)iterations;
        
        cout << "Region " << size << "x" << size << " (" << size*size << " pixels): " 
             << time_per_region << " μs/region" << endl;
    }
}

void test_sparse_vs_dense_masks() {
    cout << "\n=== Testing Sparse vs Dense Masks ===" << endl;
    
    int width = 100, height = 100;
    vector<double> densities = {0.1, 0.25, 0.5, 0.75, 1.0};
    int iterations = 100;
    
    for (double density : densities) {
        cv::Mat img(height, width, CV_8UC3);
        cv::Mat mask(height, width, CV_8UC1, cv::Scalar(0));
        
        // Create mask with specified density
        cv::randu(mask, 0, 100);
        mask = mask < (density * 100);
        mask *= 255;
        
        auto start = high_resolution_clock::now();
        uint64_t sum = 0;
        
        for (int iter = 0; iter < iterations; iter++) {
            for (int y = 0; y < height; y++) {
                const uchar* mask_row = mask.ptr<uchar>(y);
                const cv::Vec3b* img_row = img.ptr<cv::Vec3b>(y);
                int x = 0;
                
                uint32x4_t acc_sum = vdupq_n_u32(0);
                for (; x + 15 < width; x += 16) {
                    uint8x16_t mask_vec = vld1q_u8(mask_row + x);
                    
                    // Early skip check
                    uint64x2_t mask_u64 = vreinterpretq_u64_u8(mask_vec);
                    uint64_t mask_check = vgetq_lane_u64(mask_u64, 0) | vgetq_lane_u64(mask_u64, 1);
                    if (mask_check == 0) continue;
                    
                    for (int chunk = 0; chunk < 2; chunk++) {
                        int offset = x + chunk * 8;
                        uint8x8x3_t pixels = vld3_u8(reinterpret_cast<const uint8_t*>(img_row + offset));
                        uint16x8_t sum_wide = vaddq_u16(vaddq_u16(vmovl_u8(pixels.val[0]), 
                                                                   vmovl_u8(pixels.val[1])), 
                                                        vmovl_u8(pixels.val[2]));
                        acc_sum = vaddq_u32(acc_sum, vmovl_u16(vget_low_u16(sum_wide)));
                        acc_sum = vaddq_u32(acc_sum, vmovl_u16(vget_high_u16(sum_wide)));
                    }
                }
                
                for (; x < width; x++) {
                    if (mask_row[x]) {
                        sum += img_row[x][0] + img_row[x][1] + img_row[x][2];
                    }
                }
                
                uint32x2_t pair = vadd_u32(vget_low_u32(acc_sum), vget_high_u32(acc_sum));
                sum += vget_lane_u32(vpadd_u32(pair, pair), 0);
            }
        }
        
        auto end = high_resolution_clock::now();
        auto duration_us = duration_cast<microseconds>(end - start).count();
        
        cout << "Mask density " << (density * 100) << "%: " 
             << duration_us / 1000.0 << " ms for " << iterations << " iterations" << endl;
    }
}

int main() {
    cout << "=============================================" << endl;
    cout << "Bottleneck Analysis Tool" << endl;
    cout << "=============================================" << endl;
    
    cout << "\nArchitecture: arm64" << endl;
    cout << "NEON: Enabled" << endl;
    
    // Test 1: Memory bandwidth
    test_memory_bandwidth(1920, 1080, 10);
    
    // Test 2: Different region sizes
    test_region_sizes({10, 20, 50, 100, 200, 500});
    
    // Test 3: Sparse vs dense masks
    test_sparse_vs_dense_masks();
    
    cout << "\n=============================================" << endl;
    cout << "Analysis complete!" << endl;
    cout << "=============================================" << endl;
    
    return 0;
}

