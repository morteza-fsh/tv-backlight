#!/bin/bash
#
# Setup script for libcamera-based camera on Raspberry Pi
#

set -e

echo "========================================="
echo "  libcamera Setup for Raspberry Pi"
echo "========================================="
echo ""

# Color codes
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Check if running on Raspberry Pi
if [ ! -f /proc/device-tree/model ] || ! grep -q "Raspberry Pi" /proc/device-tree/model; then
    echo -e "${YELLOW}⚠ Warning:${NC} This doesn't appear to be a Raspberry Pi"
    echo "This script is designed for Raspberry Pi OS"
    read -p "Continue anyway? (y/N) " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        exit 1
    fi
fi

echo -e "${BLUE}Step 1:${NC} Checking camera"
echo "----------------------------"

if command -v libcamera-hello &>/dev/null; then
    echo -e "${GREEN}✓${NC} libcamera tools already installed"
    
    # Check if camera is detected
    if libcamera-hello --list-cameras 2>/dev/null | grep -q "Available cameras"; then
        echo -e "${GREEN}✓${NC} Camera detected:"
        libcamera-hello --list-cameras 2>/dev/null | head -10
    else
        echo -e "${RED}✗${NC} No cameras detected"
        echo "Please check:"
        echo "  1. Camera cable is firmly connected"
        echo "  2. Camera is enabled in raspi-config"
        echo "  3. You've rebooted after enabling camera"
        exit 1
    fi
else
    echo -e "${YELLOW}⚠${NC} libcamera tools not found, will install"
fi
echo ""

echo -e "${BLUE}Step 2:${NC} Installing dependencies"
echo "------------------------------------"

# Update package list
echo "Updating package list..."
sudo apt-get update -qq

# Install libcamera development files
echo "Installing libcamera-dev..."
sudo apt-get install -y libcamera-dev libcamera0 libcamera-tools

# Install OpenCV if not present
if ! pkg-config --exists opencv4; then
    echo "Installing OpenCV..."
    sudo apt-get install -y libopencv-dev
else
    echo -e "${GREEN}✓${NC} OpenCV already installed"
fi

# Install build tools
echo "Installing build tools..."
sudo apt-get install -y cmake build-essential pkg-config

echo -e "${GREEN}✓${NC} All dependencies installed"
echo ""

echo -e "${BLUE}Step 3:${NC} Verifying installation"
echo "----------------------------------"

# Check libcamera
if pkg-config --exists libcamera; then
    VERSION=$(pkg-config --modversion libcamera)
    echo -e "${GREEN}✓${NC} libcamera version: $VERSION"
else
    echo -e "${RED}✗${NC} libcamera not found by pkg-config"
    exit 1
fi

# Check OpenCV
if pkg-config --exists opencv4; then
    VERSION=$(pkg-config --modversion opencv4)
    echo -e "${GREEN}✓${NC} OpenCV version: $VERSION"
else
    echo -e "${RED}✗${NC} OpenCV not found"
    exit 1
fi

# Check camera again
echo ""
echo "Camera status:"
if libcamera-hello --list-cameras 2>/dev/null | grep -q "Available cameras"; then
    libcamera-hello --list-cameras 2>/dev/null | grep -A 5 "Available cameras"
    echo -e "${GREEN}✓${NC} Camera ready!"
else
    echo -e "${RED}✗${NC} Camera not detected"
    echo "Enable camera in raspi-config:"
    echo "  sudo raspi-config"
    echo "  → Interface Options → Camera → Enable"
    echo "  Then reboot"
    exit 1
fi
echo ""

echo -e "${BLUE}Step 4:${NC} Building project"
echo "----------------------------"

if [ ! -f CMakeLists.txt ]; then
    echo -e "${RED}✗${NC} CMakeLists.txt not found"
    echo "Please run this script from the project root directory"
    exit 1
fi

# Configure
echo "Configuring CMake..."
cmake -B build -DCMAKE_BUILD_TYPE=Debug

# Build
echo "Building project..."
cmake --build build -j$(nproc)

if [ -f build/bin/app ]; then
    echo -e "${GREEN}✓${NC} Build successful!"
    echo ""
    echo "Executable: build/bin/app"
else
    echo -e "${RED}✗${NC} Build failed"
    exit 1
fi
echo ""

echo "========================================="
echo -e "${GREEN}  Setup Complete!${NC}"
echo "========================================="
echo ""
echo "Next steps:"
echo ""
echo "1. Test with single frame:"
echo -e "   ${BLUE}./build/bin/app --live --camera /dev/video0 --single-frame --verbose${NC}"
echo ""
echo "2. Run continuous capture:"
echo -e "   ${BLUE}./build/bin/app --live --camera /dev/video0${NC}"
echo ""
echo "3. Check config.json for camera settings"
echo ""
echo -e "${GREEN}No legacy camera mode needed!${NC} This uses modern libcamera API."
echo ""

