#!/bin/bash

# Simple TV monitor - start/stop LED controller based on TV state

TV_IP="176.16.1.111"
CHECK_INTERVAL=5
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
LED_SCRIPT="$SCRIPT_DIR/run_usb_direct.sh"
LED_PID=""

echo "Monitoring TV at $TV_IP (checking every ${CHECK_INTERVAL}s)"
echo "Press Ctrl+C to stop"
echo ""

# Cleanup function
cleanup() {
    echo ""
    echo "Shutting down..."
    if [ -n "$LED_PID" ] && kill -0 "$LED_PID" 2>/dev/null; then
        echo "Stopping LED controller..."
        kill "$LED_PID" 2>/dev/null
        # Wait up to 3 seconds for graceful shutdown
        for i in {1..6}; do
            if ! kill -0 "$LED_PID" 2>/dev/null; then
                break
            fi
            sleep 0.5
        done
        # Force kill if still running
        if kill -0 "$LED_PID" 2>/dev/null; then
            echo "Force stopping LED controller..."
            kill -9 "$LED_PID" 2>/dev/null
        fi
    fi
    exit 0
}

# Set up trap for cleanup
trap cleanup EXIT INT TERM

# Main loop
TV_WAS_ON=false

while true; do
    # Check if TV is awake
    WAKEFULNESS=$(adb -s "$TV_IP:5555" shell dumpsys power 2>/dev/null | grep -i "Wakefulness")
    
    # Debug output
    echo "[$(date '+%H:%M:%S')] Wakefulness: $WAKEFULNESS"
    
    # Check if Wakefulness contains "Awake"
    if echo "$WAKEFULNESS" | grep -qi "Awake"; then
        TV_IS_ON="yes"
    else
        TV_IS_ON="no"
    fi
    
    echo "[$(date '+%H:%M:%S')] TV status: $TV_IS_ON"
    
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
            if [ -n "$LED_PID" ] && kill -0 "$LED_PID" 2>/dev/null; then
                kill "$LED_PID" 2>/dev/null
                # Don't wait indefinitely
                sleep 1
            fi
            LED_PID=""
            TV_WAS_ON=false
        fi
    fi
    
    sleep $CHECK_INTERVAL
done
