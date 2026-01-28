#!/bin/bash

# Auto TV LED Controller - Monitor TV Power State
# Automatically starts/stops LED controller based on Android TV power state

set -e

# Configuration
TV_IP="172.16.1.111"
CHECK_INTERVAL=5  # seconds between checks
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LED_SCRIPT="$SCRIPT_DIR/run_usb_direct.sh"

# Colors
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
BLUE='\033[0;34m'
NC='\033[0m'

echo -e "${BLUE}=== Auto TV LED Controller ===${NC}"
echo "Monitoring TV at: $TV_IP"
echo "Check interval: ${CHECK_INTERVAL}s"
echo "Press Ctrl+C to stop monitoring"
echo ""

# PID of the LED script process
LED_PID=""

# Function to check if TV is awake
check_tv_awake() {
    local result=$(adb -s "$TV_IP:5555" shell dumpsys power 2>/dev/null | grep -i "Wakefulness" | grep -i "Awake")
    if [ -n "$result" ]; then
        return 0  # TV is awake
    else
        return 1  # TV is asleep or unreachable
    fi
}

# Function to check ADB connection
ensure_adb_connected() {
    # Check if device is already connected
    if adb devices | grep -q "$TV_IP:5555"; then
        return 0
    fi
    
    # Try to connect
    adb connect "$TV_IP:5555" >/dev/null 2>&1
    sleep 1
    
    # Verify connection
    if adb devices | grep -q "$TV_IP:5555"; then
        return 0
    else
        return 1
    fi
}

# Function to turn off LEDs
turn_leds_off() {
    echo -e "${YELLOW}■ TV is OFF - Turning LEDs off${NC}"
    
    # Get config and USB device info
    CONFIG_FILE="$SCRIPT_DIR/config.hyperhdr.json"
    
    if [ ! -f "$CONFIG_FILE" ]; then
        echo -e "${RED}  ✗ Config file not found: $CONFIG_FILE${NC}"
        return 1
    fi
    
    USB_DEVICE=$(grep -A 3 '"usb"' "$CONFIG_FILE" | grep '"device"' | cut -d'"' -f4)
    USB_BAUDRATE=$(grep -A 3 '"usb"' "$CONFIG_FILE" | grep '"baudrate"' | grep -o '[0-9]*')
    LED_COUNT=$(grep -A 5 '"hyperhdr"' "$CONFIG_FILE" | grep -E '"top"|"bottom"|"left"|"right"' | grep -o '[0-9]*' | awk '{sum+=$1} END {print sum}')
    
    echo "  Debug: USB_DEVICE=$USB_DEVICE, BAUDRATE=$USB_BAUDRATE, LED_COUNT=$LED_COUNT"
    
    if [ -z "$LED_COUNT" ] || [ "$LED_COUNT" -eq 0 ]; then
        echo -e "${RED}  ✗ LED count is 0 or not found in config${NC}"
        return 1
    fi
    
    if [ ! -e "$USB_DEVICE" ]; then
        echo -e "${RED}  ✗ USB device not found: $USB_DEVICE${NC}"
        return 1
    fi
    
    python3 -c "
import sys
import time
try:
    import serial
    led_count = $LED_COUNT
    print(f'  Sending OFF command: {led_count} LEDs to $USB_DEVICE @ $USB_BAUDRATE baud', file=sys.stderr)
    ser = serial.Serial('$USB_DEVICE', $USB_BAUDRATE, timeout=1)
    time.sleep(0.1)
    
    hi = (led_count - 1) >> 8
    lo = (led_count - 1) & 0xFF
    checksum = hi ^ lo ^ 0x55
    
    header = bytearray([ord('A'), ord('d'), ord('a'), hi, lo, checksum])
    leds = bytearray([0, 0, 0] * led_count)
    
    ser.write(header + leds)
    ser.flush()
    time.sleep(0.1)
    ser.close()
    print('  ✓ LEDs turned off successfully', file=sys.stderr)
except ImportError:
    print('  ✗ pyserial not installed. Install with: pip3 install pyserial', file=sys.stderr)
    sys.exit(1)
except Exception as e:
    print(f'  ✗ Error turning off LEDs: {e}', file=sys.stderr)
    sys.exit(1)
" 2>&1
    
    if [ $? -ne 0 ]; then
        echo -e "${RED}  ✗ Failed to turn off LEDs${NC}"
        return 1
    fi
}

# Function to start LED script
start_led_script() {
    if [ -n "$LED_PID" ] && kill -0 "$LED_PID" 2>/dev/null; then
        return 0  # Already running
    fi
    
    echo -e "${GREEN}▶ Starting LED controller${NC}"
    "$LED_SCRIPT" > /tmp/tv_led_output.log 2>&1 &
    LED_PID=$!
    echo "  LED script PID: $LED_PID"
}

# Function to stop LED script (only on cleanup)
stop_led_script() {
    if [ -z "$LED_PID" ]; then
        return 0
    fi
    
    if kill -0 "$LED_PID" 2>/dev/null; then
        echo -e "${YELLOW}Stopping LED controller${NC}"
        kill -TERM "$LED_PID" 2>/dev/null || true
        wait "$LED_PID" 2>/dev/null || true
        LED_PID=""
    else
        LED_PID=""
    fi
}

# Cleanup on script exit
cleanup() {
    echo ""
    echo -e "${YELLOW}Shutting down monitor...${NC}"
    stop_led_script
    echo -e "${GREEN}Done${NC}"
    exit 0
}

trap cleanup EXIT INT TERM

# Start LED script once
start_led_script
echo ""

# Main monitoring loop
TV_STATE="unknown"

while true; do
    # Ensure ADB is connected
    if ! ensure_adb_connected; then
        if [ "$TV_STATE" != "disconnected" ]; then
            echo -e "${RED}✗ Cannot connect to TV at $TV_IP${NC}"
            echo "  Make sure ADB is enabled on the TV"
            TV_STATE="disconnected"
            turn_leds_off
        fi
        sleep "$CHECK_INTERVAL"
        continue
    fi
    
    # Check TV power state
    if check_tv_awake; then
        if [ "$TV_STATE" != "awake" ]; then
            echo -e "${GREEN}✓ TV is ON - LEDs active${NC}"
            TV_STATE="awake"
        fi
    else
        if [ "$TV_STATE" != "asleep" ]; then
            TV_STATE="asleep"
            turn_leds_off
        fi
    fi
    
    # Check if LED script is still running (restart if crashed)
    if [ -n "$LED_PID" ]; then
        if ! kill -0 "$LED_PID" 2>/dev/null; then
            echo -e "${YELLOW}⚠ LED script stopped unexpectedly, restarting...${NC}"
            LED_PID=""
            start_led_script
        fi
    fi
    
    sleep "$CHECK_INTERVAL"
done
