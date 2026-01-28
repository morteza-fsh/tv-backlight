#!/bin/bash

# Simple TV monitor - start/stop LED controller based on TV state

TV_IP="176.16.1.111"
CHECK_INTERVAL=5
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LED_SCRIPT="$SCRIPT_DIR/run_usb_direct.sh"
CONFIG_FILE="$SCRIPT_DIR/config.hyperhdr.json"
LED_PID=""

echo "Monitoring TV at $TV_IP (checking every ${CHECK_INTERVAL}s)"
echo "Press Ctrl+C to stop"
echo ""

# Function to turn off LEDs
turn_off_leds() {
    if [ -f "$CONFIG_FILE" ]; then
        USB_DEVICE=$(grep -A 3 '"usb"' "$CONFIG_FILE" | grep '"device"' | cut -d'"' -f4)
        USB_BAUDRATE=$(grep -A 3 '"usb"' "$CONFIG_FILE" | grep '"baudrate"' | grep -o '[0-9]*')
        LED_COUNT=$(grep -A 5 '"hyperhdr"' "$CONFIG_FILE" | grep -E '"top"|"bottom"|"left"|"right"' | grep -o '[0-9]*' | awk '{sum+=$1} END {print sum}')
        
        if [ -n "$LED_COUNT" ] && [ "$LED_COUNT" -gt 0 ] && [ -e "$USB_DEVICE" ]; then
            if command -v python3 >/dev/null 2>&1; then
                python3 "$SCRIPT_DIR/turn_off_leds.py" "$USB_DEVICE" "$USB_BAUDRATE" "$LED_COUNT" 2>/dev/null
            fi
        fi
    fi
}

# Kill LED script and turn off LEDs on exit
cleanup() {
    if [ -n "$LED_PID" ]; then
        kill $LED_PID 2>/dev/null
        wait $LED_PID 2>/dev/null
    fi
    turn_off_leds
}

trap cleanup EXIT INT TERM

# Main loop
TV_WAS_ON=false

while true; do
    # Check if TV is awake
    TV_IS_ON=$(adb -s "$TV_IP:5555" shell dumpsys power 2>/dev/null | grep -i "Wakefulness.*Awake" && echo "yes" || echo "no")
    
    if [ "$TV_IS_ON" = "yes" ]; then
        if [ "$TV_WAS_ON" = false ]; then
            echo "TV ON - Starting LEDs"
            "$LED_SCRIPT" > /tmp/tv_led.log 2>&1 &
            LED_PID=$!
            TV_WAS_ON=true
        fi
    else
        if [ "$TV_WAS_ON" = true ]; then
            echo "TV OFF - Stopping LEDs"
            if [ -n "$LED_PID" ]; then
                kill $LED_PID 2>/dev/null
                wait $LED_PID 2>/dev/null
            fi
            sleep 1
            turn_off_leds
            LED_PID=""
            TV_WAS_ON=false
        fi
    fi
    
    sleep $CHECK_INTERVAL
done
