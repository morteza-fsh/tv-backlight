#!/bin/bash

# Profiling Tool for LED Controller
# This script helps identify bottlenecks in your processing pipeline

echo "============================================="
echo "LED Controller Profiling Tool"
echo "============================================="
echo ""

cd "$(dirname "$0")"

# Check if build exists
if [ ! -f "build/bin/app" ]; then
    echo "Error: Application not built. Please run:"
    echo "  mkdir -p build && cd build && cmake .. && make"
    exit 1
fi

# Apply profiling patch
echo "Would you like to apply the profiling patch? (y/n)"
echo "This adds detailed timing to your code."
read -r response

if [[ "$response" =~ ^[Yy]$ ]]; then
    echo "Applying profiling patch..."
    patch -p1 < add_profiling.patch
    
    if [ $? -eq 0 ]; then
        echo "✅ Patch applied successfully"
        echo "Rebuilding..."
        cd build && make
        cd ..
    else
        echo "❌ Patch failed. It may already be applied."
        echo "To revert: git checkout src/processing/ColorExtractor.cpp src/core/LEDController.cpp"
    fi
fi

echo ""
echo "============================================="
echo "Running Profiling Tests"
echo "============================================="
echo ""

# Test 1: Single frame profiling
echo "Test 1: Single Frame Analysis"
echo "------------------------------"
if [ -f "img.png" ]; then
    echo "Running single frame test..."
    ./build/bin/app --debug --image img.png --single-frame --verbose 2>&1 | tee profile_single.log
    
    echo ""
    echo "Results saved to profile_single.log"
    echo ""
    echo "Time Breakdown:"
    grep "Time Breakdown" -A 5 profile_single.log
else
    echo "⚠️  No img.png found, skipping single frame test"
fi

echo ""
echo "Test 2: Multi-frame Analysis (10 frames)"
echo "----------------------------------------"
if [ -f "img.png" ]; then
    echo "Running 10 frame test..."
    
    # Modify config to run 10 frames then exit
    timeout 10s ./build/bin/app --debug --verbose 2>&1 | tee profile_multi.log
    
    echo ""
    echo "Results saved to profile_multi.log"
    echo ""
    echo "Average times:"
    echo "  Frame I/O:"
    grep "Frame I/O:" profile_multi.log | awk '{sum+=$3; count++} END {print "    Avg:", sum/count, "μs"}'
    echo "  Color Extraction:"
    grep "Color Extraction:" profile_multi.log | awk '{sum+=$3; count++} END {print "    Avg:", sum/count, "μs"}'
    echo "  Network I/O:"
    grep "Network I/O:" profile_multi.log | awk '{sum+=$3; count++} END {print "    Avg:", sum/count, "μs"}'
    echo "  Total:"
    grep "Frame processed in" profile_multi.log | awk '{sum+=$4; count++} END {print "    Avg:", sum/count, "ms"}'
fi

echo ""
echo "============================================="
echo "Test 3: Region Size Analysis"
echo "============================================="
echo ""
echo "Analyzing typical region sizes..."

# Extract region info from config
if [ -f "config.json" ]; then
    echo "Current config:"
    grep -A 10 "color_extraction" config.json | head -n 15
    
    echo ""
    echo "Estimated region sizes:"
    python3 << 'EOF'
import json
try:
    with open('config.json') as f:
        config = json.load(f)
    
    # Assuming 1920x1080 frame
    width, height = 1920, 1080
    
    mode = config.get('color_extraction', {}).get('mode', 'grid')
    
    if mode == 'edge_slices':
        h_cov = config.get('color_extraction', {}).get('horizontal_coverage_percent', 20) / 100.0
        v_cov = config.get('color_extraction', {}).get('vertical_coverage_percent', 15) / 100.0
        h_slices = config.get('color_extraction', {}).get('horizontal_slices', 10)
        v_slices = config.get('color_extraction', {}).get('vertical_slices', 8)
        
        print(f"  Mode: edge_slices")
        print(f"  Top/Bottom region size: ~{width//h_slices} x {int(height*h_cov)} = {(width//h_slices)*int(height*h_cov)} pixels")
        print(f"  Left/Right region size: ~{int(width*v_cov)} x {height//v_slices} = {int(width*v_cov)*(height//v_slices)} pixels")
    else:
        rows = config.get('led_layout', {}).get('grid_rows', 5)
        cols = config.get('led_layout', {}).get('grid_cols', 8)
        print(f"  Mode: grid")
        print(f"  Region size: ~{width//cols} x {height//rows} = {(width//cols)*(height//rows)} pixels")
except:
    print("  Could not parse config.json")
EOF
fi

echo ""
echo "============================================="
echo "Analysis Complete!"
echo "============================================="
echo ""
echo "Summary:"
echo "--------"
echo "Review the logs above to identify bottlenecks:"
echo ""
echo "1. If 'Frame I/O' is >40%, optimize:"
echo "   - Reduce camera resolution"
echo "   - Use camera scaling"
echo "   - Disable auto-focus/auto-white-balance"
echo ""
echo "2. If 'Color Extraction' is >30%, optimize:"
echo "   - Reduce number of regions"
echo "   - Reduce polygon samples"
echo "   - Check region sizes (small = inefficient)"
echo ""
echo "3. If 'Network I/O' is >30%, optimize:"
echo "   - Use linear format"
echo "   - Reduce update frequency"
echo "   - Consider UDP instead of TCP"
echo ""
echo "4. If regions are <1000 pixels each:"
echo "   - NEON overhead is significant"
echo "   - Consider larger regions or fewer slices"
echo ""
echo "For detailed analysis, see:"
echo "  - profile_single.log"
echo "  - profile_multi.log"
echo "  - BOTTLENECK_ANALYSIS.md"
echo ""
echo "To revert profiling changes:"
echo "  git checkout src/processing/ColorExtractor.cpp src/core/LEDController.cpp"
echo "  cd build && make"
echo ""

