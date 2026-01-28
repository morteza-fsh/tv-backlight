#!/bin/bash

# USB Direct Mode - Quick Run Script
# This script helps you run the TV LED controller with USB direct output

set -e

# Colors for output
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m' # No Color

echo -e "${GREEN}=== TV LED Controller - USB Direct Mode ===${NC}"
echo ""

# Configuration
CONFIG_FILE="config.hyperhdr.json"
APP_PATH="./build/bin/app"
USB_DEVICE="/dev/ttyUSB0"

# Check if app exists
if [ ! -f "$APP_PATH" ]; then
    echo -e "${RED}Error: Application not found at $APP_PATH${NC}"
    echo "Please build the project first:"
    echo "  cd build && cmake .. && make"
    exit 1
fi

# Check if config exists
if [ ! -f "$CONFIG_FILE" ]; then
    echo -e "${RED}Error: Config file not found: $CONFIG_FILE${NC}"
    exit 1
fi

# Check USB device (if on Linux)
if [ -e "$USB_DEVICE" ]; then
    echo -e "${GREEN}✓ USB device found: $USB_DEVICE${NC}"
    
    # Check permissions
    if [ ! -r "$USB_DEVICE" ] || [ ! -w "$USB_DEVICE" ]; then
        echo -e "${YELLOW}⚠ Warning: Permission issues with $USB_DEVICE${NC}"
        echo "  Try: sudo chmod 666 $USB_DEVICE"
        echo "  Or add your user to dialout group: sudo usermod -a -G dialout \$USER"
    else
        echo -e "${GREEN}✓ USB device is readable/writable${NC}"
    fi
else
    echo -e "${YELLOW}⚠ USB device not found: $USB_DEVICE${NC}"
    echo "  Available USB devices:"
    ls -l /dev/ttyUSB* /dev/ttyACM* 2>/dev/null || echo "    (none found)"
    echo ""
    echo "  Please update the device path in $CONFIG_FILE"
fi

echo ""
echo -e "${GREEN}Configuration:${NC}"
echo "  Config file: $CONFIG_FILE"
echo "  Mode: Live camera"
echo "  Output: USB Direct (Arduino/ESP32)"

# Extract USB settings from config
USB_ENABLED=$(grep -A 3 '"usb"' "$CONFIG_FILE" | grep '"enabled"' | grep -o 'true\|false')
USB_DEVICE=$(grep -A 3 '"usb"' "$CONFIG_FILE" | grep '"device"' | cut -d'"' -f4)
USB_BAUDRATE=$(grep -A 3 '"usb"' "$CONFIG_FILE" | grep '"baudrate"' | grep -o '[0-9]*')

if [ "$USB_ENABLED" = "true" ]; then
    echo -e "${GREEN}  USB: Enabled${NC}"
    echo "  Device: $USB_DEVICE"
    echo "  Baudrate: $USB_BAUDRATE"
    
    # Calculate expected performance
    LED_COUNT=$(grep -A 5 '"hyperhdr"' "$CONFIG_FILE" | grep -E '"top"|"bottom"|"left"|"right"' | grep -o '[0-9]*' | awk '{sum+=$1} END {print sum}')
    if [ -n "$LED_COUNT" ] && [ "$LED_COUNT" -gt 0 ]; then
        PACKET_SIZE=$((6 + LED_COUNT * 3))
        MAX_FPS=$((USB_BAUDRATE / (PACKET_SIZE * 10)))
        echo "  LED Count: $LED_COUNT"
        echo "  Packet Size: $PACKET_SIZE bytes"
        echo "  Max FPS: ~$MAX_FPS"
    fi
else
    echo -e "${RED}  USB: Disabled${NC}"
    echo "  Please enable USB in $CONFIG_FILE"
    exit 1
fi

echo ""
echo -e "${GREEN}Starting application...${NC}"
echo "  Press Ctrl+C to stop"
echo ""

# Function to turn off all LEDs
cleanup_leds() {
    echo ""
    echo -e "${YELLOW}Turning off all LEDs...${NC}"
    
    LED_COUNT=$(grep -A 5 '"hyperhdr"' "$CONFIG_FILE" | grep -E '"top"|"bottom"|"left"|"right"' | grep -o '[0-9]*' | awk '{sum+=$1} END {print sum}')
    
    if [ -n "$LED_COUNT" ] && [ "$LED_COUNT" -gt 0 ] && [ -e "$USB_DEVICE" ]; then
        stty -F "$USB_DEVICE" "$USB_BAUDRATE" raw -echo 2>/dev/null
        local hi=$(( (LED_COUNT - 1) >> 8 ))
        local lo=$(( (LED_COUNT - 1) & 0xFF ))
        local chk=$(( (hi ^ lo ^ 0x55) & 0xFF ))
        printf "Ada\\x$(printf '%02x' $hi)\\x$(printf '%02x' $lo)\\x$(printf '%02x' $chk)$(printf '\\x00%.0s' $(seq 1 $((LED_COUNT * 3))))" > "$USB_DEVICE" 2>/dev/null && echo "✓ LEDs turned off"
    fi
}

# Set up trap to clean up on exit
trap cleanup_leds EXIT INT TERM

# Run the application
"$APP_PATH" \
    --config "$CONFIG_FILE" \
    --live \
    --verbose

