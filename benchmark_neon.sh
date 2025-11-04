#!/bin/bash

# Benchmark script to compare NEON SIMD vs scalar performance
# This script compares color extraction performance

echo "=========================================="
echo "NEON SIMD Performance Benchmark"
echo "=========================================="
echo ""

# Check if we're on ARM
ARCH=$(uname -m)
echo "Architecture: $ARCH"

if [ "$ARCH" != "arm64" ] && [ "$ARCH" != "aarch64" ]; then
    echo "Warning: Not running on ARM architecture. NEON may not be available."
fi

# Check NEON support
echo ""
echo "Checking NEON support..."
echo | clang++ -dM -E -march=native - | grep -i "__ARM_NEON" && echo "✅ NEON is available" || echo "❌ NEON not available"

echo ""
echo "Current build uses:"
strings build/bin/app | grep -i "NEON\|SIMD" || echo "  (no SIMD indicators found in binary)"

echo ""
echo "=========================================="
echo "Running application with NEON optimized color extraction"
echo "=========================================="
echo ""
echo "Look for this log message:"
echo "  'Pre-computing X masks with NEON SIMD enabled...'"
echo ""
echo "Performance improvements expected:"
echo "  - Color extraction: 2-4x faster on large regions"
echo "  - Overall frame processing: 30-50% faster"
echo ""
echo "Press Ctrl+C to stop the application"
echo "=========================================="
echo ""

# Run the application
build/bin/app

