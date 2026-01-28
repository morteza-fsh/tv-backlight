#!/bin/bash

# Auto TV LED Controller - Monitor TV Power State
# Automatically starts/stops LED controller based on Android TV power state

set -e

# Configuration
TV_IP="176.16.1.111"
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

# Function to start LED script
start_led_script() {
    if [ -n "$LED_PID" ] && kill -0 "$LED_PID" 2>/dev/null; then
        return 0  # Already running
    fi
    
    echo -e "${GREEN}▶ TV is ON - Starting LED controller${NC}"
    "$LED_SCRIPT" > /tmp/tv_led_output.log 2>&1 &
    LED_PID=$!
    echo "  LED script PID: $LED_PID"
}

# Function to stop LED script
stop_led_script() {
    if [ -z "$LED_PID" ]; then
        return 0  # Not running
    fi
    
    if kill -0 "$LED_PID" 2>/dev/null; then
        echo -e "${YELLOW}■ TV is OFF - Stopping LED controller${NC}"
        kill -TERM "$LED_PID" 2>/dev/null || true
        wait "$LED_PID" 2>/dev/null || true
        LED_PID=""
        echo "  LED controller stopped"
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

# Main monitoring loop
TV_STATE="unknown"

while true; do
    # Ensure ADB is connected
    if ! ensure_adb_connected; then
        if [ "$TV_STATE" != "disconnected" ]; then
            echo -e "${RED}✗ Cannot connect to TV at $TV_IP${NC}"
            echo "  Make sure ADB is enabled on the TV"
            TV_STATE="disconnected"
            stop_led_script
        fi
        sleep "$CHECK_INTERVAL"
        continue
    fi
    
    # Check TV power state
    if check_tv_awake; then
        if [ "$TV_STATE" != "awake" ]; then
            TV_STATE="awake"
            start_led_script
        fi
    else
        if [ "$TV_STATE" != "asleep" ]; then
            TV_STATE="asleep"
            stop_led_script
        fi
    fi
    
    # Check if LED script is still running (may have crashed)
    if [ "$TV_STATE" = "awake" ] && [ -n "$LED_PID" ]; then
        if ! kill -0 "$LED_PID" 2>/dev/null; then
            echo -e "${YELLOW}⚠ LED script stopped unexpectedly, restarting...${NC}"
            LED_PID=""
            start_led_script
        fi
    fi
    
    sleep "$CHECK_INTERVAL"
done
