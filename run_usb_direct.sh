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
CONFIG_FILE="config.json"
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
    LED_COUNT=$(grep -A 20 '"led_layout"' "$CONFIG_FILE" | grep -A 6 '"hyperhdr"' | grep -E '"top"|"bottom"|"left"|"right"' | grep -o '[0-9]*' | awk '{sum+=$1} END {print sum}')
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
    
    # Kill the application if it's still running
    if [ -n "$APP_PID" ]; then
        kill "$APP_PID" 2>/dev/null || true
        # Give it time to release the serial port
        sleep 1
    fi
    
    LED_COUNT=$(grep -A 20 '"led_layout"' "$CONFIG_FILE" | grep -A 6 '"hyperhdr"' | grep -E '"top"|"bottom"|"left"|"right"' | grep -o '[0-9]*' | awk '{sum+=$1} END {print sum}')
    
    if [ -n "$LED_COUNT" ] && [ "$LED_COUNT" -gt 0 ] && [ -e "$USB_DEVICE" ]; then
        # Use Python for reliable serial communication
        python3 -c "
import sys
import time

try:
    import serial
    ser = serial.Serial('$USB_DEVICE', $USB_BAUDRATE, timeout=1)
    time.sleep(0.1)  # Let port stabilize
    
    # Prepare Adalight packet: 'Ada' + hi + lo + checksum + RGB data (all zeros)
    led_count = $LED_COUNT
    hi = (led_count - 1) >> 8
    lo = (led_count - 1) & 0xFF
    chk = (hi ^ lo ^ 0x55) & 0xFF
    
    packet = b'Ada' + bytes([hi, lo, chk]) + (b'\\x00' * (led_count * 3))
    
    # Send multiple times to ensure receipt
    for _ in range(3):
        ser.write(packet)
        ser.flush()
        time.sleep(0.05)
    
    ser.close()
    print('✓ LEDs turned off')
except ImportError:
    print('⚠ Python serial module not installed. Install with: pip3 install pyserial')
    sys.exit(1)
except Exception as e:
    print(f'⚠ Failed to turn off LEDs: {e}')
    sys.exit(1)
" || echo "⚠ Python cleanup failed, trying shell method..."
    fi
}

# Set up trap to clean up on exit
trap cleanup_leds EXIT INT TERM

# Run the application in background and capture PID
"$APP_PATH" \
    --config "$CONFIG_FILE" \
    --live \
    --verbose &

APP_PID=$!

# Wait for the application to finish
wait "$APP_PID" 2>/dev/null || true

