#!/bin/bash
#
# Camera Diagnostic Script for Raspberry Pi
# Helps diagnose why OpenCV can't read from the camera
#

set -e

echo "========================================="
echo "   Raspberry Pi Camera Diagnostics"
echo "========================================="
echo ""

# Color codes
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Check 1: Is this a Raspberry Pi?
echo "1. System Check"
echo "   ------------"
if [ -f /proc/device-tree/model ]; then
    MODEL=$(cat /proc/device-tree/model)
    echo -e "   ${GREEN}✓${NC} Raspberry Pi detected: $MODEL"
else
    echo -e "   ${YELLOW}⚠${NC} Not a Raspberry Pi or can't detect model"
fi
echo ""

# Check 2: Does /dev/video0 exist?
echo "2. Video Device Check"
echo "   ------------------"
if [ -e /dev/video0 ]; then
    echo -e "   ${GREEN}✓${NC} /dev/video0 exists"
    ls -l /dev/video0
    
    # Check permissions
    if [ -r /dev/video0 ] && [ -w /dev/video0 ]; then
        echo -e "   ${GREEN}✓${NC} You have read/write access to /dev/video0"
    else
        echo -e "   ${RED}✗${NC} No read/write access to /dev/video0"
        echo "   Fix: sudo usermod -a -G video $USER"
        echo "        Then log out and back in"
    fi
else
    echo -e "   ${RED}✗${NC} /dev/video0 does not exist"
    echo "   This means no camera is detected"
fi
echo ""

# Check 3: List all video devices
echo "3. All Video Devices"
echo "   ------------------"
if command -v v4l2-ctl &> /dev/null; then
    echo "   Available video devices:"
    v4l2-ctl --list-devices || echo "   No video devices found"
else
    echo -e "   ${YELLOW}⚠${NC} v4l2-ctl not installed"
    echo "   Install: sudo apt-get install v4l-utils"
fi
echo ""

# Check 4: Can we query /dev/video0?
echo "4. Camera Capabilities Check"
echo "   -------------------------"
if [ -e /dev/video0 ] && command -v v4l2-ctl &> /dev/null; then
    echo "   Supported formats:"
    v4l2-ctl -d /dev/video0 --list-formats-ext 2>&1 | head -30 || {
        echo -e "   ${RED}✗${NC} Cannot query camera formats"
        echo "   This usually means:"
        echo "   - Camera uses libcamera (not V4L2)"
        echo "   - Need to enable legacy camera support"
    }
else
    echo -e "   ${YELLOW}⚠${NC} Skipped (no device or v4l2-ctl not installed)"
fi
echo ""

# Check 5: Is legacy camera enabled?
echo "5. Legacy Camera Status"
echo "   --------------------"
if [ -f /boot/config.txt ]; then
    if grep -q "^start_x=1" /boot/config.txt || grep -q "^camera_auto_detect=0" /boot/config.txt; then
        echo -e "   ${GREEN}✓${NC} Legacy camera appears to be enabled in /boot/config.txt"
    else
        echo -e "   ${YELLOW}⚠${NC} Legacy camera may not be enabled"
        echo "   Check: grep -E 'start_x|camera_auto_detect' /boot/config.txt"
    fi
elif [ -f /boot/firmware/config.txt ]; then
    if grep -q "^start_x=1" /boot/firmware/config.txt || grep -q "^camera_auto_detect=0" /boot/firmware/config.txt; then
        echo -e "   ${GREEN}✓${NC} Legacy camera appears to be enabled in /boot/firmware/config.txt"
    else
        echo -e "   ${YELLOW}⚠${NC} Legacy camera may not be enabled"
        echo "   Check: grep -E 'start_x|camera_auto_detect' /boot/firmware/config.txt"
    fi
else
    echo -e "   ${YELLOW}⚠${NC} Cannot find config.txt"
fi
echo ""

# Check 6: libcamera tools
echo "6. Libcamera Check"
echo "   ---------------"
if command -v libcamera-hello &> /dev/null; then
    echo -e "   ${GREEN}✓${NC} libcamera tools installed"
    echo "   Testing camera with libcamera:"
    timeout 2 libcamera-hello --list-cameras 2>&1 || echo "   (timed out or no cameras)"
else
    echo -e "   ${YELLOW}⚠${NC} libcamera tools not installed"
fi
echo ""

# Check 7: Is camera in use?
echo "7. Camera Usage Check"
echo "   ------------------"
if [ -e /dev/video0 ]; then
    USERS=$(sudo fuser /dev/video0 2>/dev/null || true)
    if [ -z "$USERS" ]; then
        echo -e "   ${GREEN}✓${NC} Camera is not currently in use"
    else
        echo -e "   ${RED}✗${NC} Camera is being used by process(es): $USERS"
        ps -p $USERS || true
    fi
else
    echo "   Skipped (no /dev/video0)"
fi
echo ""

# Summary and recommendations
echo "========================================="
echo "   SUMMARY & RECOMMENDATIONS"
echo "========================================="
echo ""

if [ ! -e /dev/video0 ]; then
    echo -e "${RED}ISSUE:${NC} No video device found"
    echo ""
    echo "If you have a Raspberry Pi Camera Module:"
    echo "  1. Enable legacy camera support:"
    echo "     sudo raspi-config"
    echo "     → Interface Options → Legacy Camera → Enable"
    echo "  2. Reboot: sudo reboot"
    echo ""
    echo "If you have a USB camera:"
    echo "  1. Check it's connected: lsusb"
    echo "  2. Check dmesg for errors: dmesg | grep -i usb"
    
elif ! v4l2-ctl -d /dev/video0 --list-formats-ext &>/dev/null; then
    echo -e "${RED}ISSUE:${NC} Video device exists but can't be queried"
    echo ""
    echo "This is THE most common issue with Raspberry Pi Camera Module."
    echo "The camera uses libcamera, but OpenCV needs V4L2."
    echo ""
    echo -e "${GREEN}SOLUTION:${NC}"
    echo "  1. Enable legacy camera support:"
    echo "     sudo raspi-config"
    echo "     → Interface Options → Legacy Camera → Enable"
    echo "  2. Reboot: sudo reboot"
    echo "  3. Re-run this diagnostic script"
    echo ""
    echo "After reboot, v4l2-ctl should be able to query the camera."
    
else
    echo -e "${GREEN}SUCCESS:${NC} Camera appears to be properly configured!"
    echo ""
    echo "Your camera should work with OpenCV."
    echo "If you still have issues, try:"
    echo "  - Lower resolution (640x480 instead of 1920x1080)"
    echo "  - Check config.json camera settings"
    echo "  - Run app with --verbose flag for detailed logs"
fi

echo ""
echo "========================================="
echo ""

