#!/bin/bash
#
# Simple setup script for Raspberry Pi camera
# Uses rpicam-vid pipe method - NO complex libraries needed!
#

set -e

echo "========================================="
echo "  Simple Camera Setup for Raspberry Pi"
echo "========================================="
echo ""

# Color codes
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}Step 1:${NC} Installing dependencies"
echo "------------------------------------"

# Update package list (ignore errors from broken repos)
sudo apt-get update 2>&1 | grep -v "Release file" || true

# Remove broken uv4l repo if it exists
sudo sed -i '/uv4l/d' /etc/apt/sources.list.d/*.list 2>/dev/null || true

# Clean up
sudo apt-get clean

# Install rpicam-apps (provides rpicam-vid)
echo "Installing rpicam-apps (camera utilities)..."
sudo apt-get install -y rpicam-apps

# Install OpenCV (for image processing)
echo "Installing OpenCV..."
sudo apt-get install -y libopencv-dev

# Install build tools
echo "Installing build tools..."
sudo apt-get install -y cmake build-essential

echo -e "${GREEN}✓${NC} All dependencies installed"
echo ""

echo -e "${BLUE}Step 2:${NC} Verifying camera"
echo "----------------------------"

if rpicam-hello --list-cameras 2>/dev/null | grep -q "Available cameras"; then
    echo "Camera detected:"
    rpicam-hello --list-cameras 2>/dev/null | head -10
    echo -e "${GREEN}✓${NC} Camera ready!"
else
    echo -e "${YELLOW}⚠${NC} Camera not detected"
    echo "Please check:"
    echo "  1. Camera cable is firmly connected"
    echo "  2. Camera is enabled: sudo raspi-config -> Interface Options -> Camera"
    echo "  3. Reboot after enabling: sudo reboot"
fi
echo ""

echo -e "${BLUE}Step 3:${NC} Building project"
echo "----------------------------"

if [ ! -f CMakeLists.txt ]; then
    echo "Error: CMakeLists.txt not found"
    echo "Please run this script from the project root directory"
    exit 1
fi

# Configure and build
echo "Configuring CMake..."
cmake -B build -DCMAKE_BUILD_TYPE=Debug

echo "Building..."
cmake --build build -j$(nproc)

if [ -f build/bin/app ]; then
    echo -e "${GREEN}✓${NC} Build successful!"
else
    echo "Build failed"
    exit 1
fi
echo ""

echo "========================================="
echo -e "${GREEN}  Setup Complete!${NC}"
echo "========================================="
echo ""
echo "Test the camera:"
echo -e "  ${BLUE}./build/bin/app --live --camera 0 --single-frame --verbose${NC}"
echo ""
echo "What makes this simple:"
echo "  ✓ No complex libraries to install"
echo "  ✓ No legacy mode needed"
echo "  ✓ Just pipes from rpicam-vid"
echo "  ✓ Low latency"
echo ""

