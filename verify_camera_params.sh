#!/bin/bash
# Camera Parameter Verification Script
# Run this on your Raspberry Pi and send the output back

echo "=== Camera Parameter Verification ==="
echo ""

# Check rpicam-vid version
echo "1. rpicam-vid version:"
rpicam-vid --version 2>&1 || echo "rpicam-vid not found"
echo ""

# Get full help output
echo "2. Full rpicam-vid help (all supported parameters):"
rpicam-vid --help 2>&1
echo ""

# Check specific parameters we're using
echo "3. Checking specific parameters we're trying to use:"
echo ""

echo "Testing --gain:"
rpicam-vid --gain 1.0 --timeout 1 --nopreview -o /dev/null 2>&1 | head -5
echo ""

echo "Testing --digital-gain:"
rpicam-vid --digital-gain 1.0 --timeout 1 --nopreview -o /dev/null 2>&1 | head -5
echo ""

echo "Testing --exposure:"
rpicam-vid --exposure 30000 --timeout 1 --nopreview -o /dev/null 2>&1 | head -5
echo ""

echo "Testing --awb-temperature:"
rpicam-vid --awb-temperature 5000 --timeout 1 --nopreview -o /dev/null 2>&1 | head -5
echo ""

echo "Testing --ccm:"
rpicam-vid --ccm "1,0,0,0,1,0,0,0,1" --timeout 1 --nopreview -o /dev/null 2>&1 | head -5
echo ""

# List connected cameras
echo "4. Connected cameras:"
rpicam-hello --list-cameras 2>&1
echo ""

# Show camera controls (if available)
echo "5. Available camera controls (v4l2):"
if command -v v4l2-ctl &> /dev/null; then
    v4l2-ctl --list-ctrls 2>&1 | head -50
else
    echo "v4l2-ctl not installed (optional)"
fi
echo ""

echo "=== End of verification ==="

