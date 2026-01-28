#!/bin/bash

# TV LED Monitor Service Control Script

SERVICE_NAME="tv-led-monitor.service"
SERVICE_FILE="$(cd "$(dirname "$0")" && pwd)/tv-led-monitor.service"

# Colors
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

show_usage() {
    echo "Usage: $0 {install|uninstall|start|stop|restart|status|logs|enable|disable|check}"
    echo ""
    echo "Commands:"
    echo "  install    - Install and enable the service"
    echo "  uninstall  - Stop, disable and remove the service"
    echo "  start      - Start the service"
    echo "  stop       - Stop the service"
    echo "  restart    - Restart the service"
    echo "  status     - Show service status"
    echo "  logs       - Show live logs (Ctrl+C to exit)"
    echo "  enable     - Enable service to start on boot"
    echo "  disable    - Disable service from starting on boot"
    echo "  check      - Check if service is installed and running"
}

check_root() {
    if [ "$EUID" -ne 0 ]; then 
        echo -e "${RED}Error: This command requires sudo${NC}"
        echo "Run: sudo $0 $1"
        exit 1
    fi
}

case "$1" in
    install)
        check_root "$1"
        echo -e "${BLUE}Installing TV LED Monitor service...${NC}"
        
        if [ ! -f "$SERVICE_FILE" ]; then
            echo -e "${RED}Error: Service file not found: $SERVICE_FILE${NC}"
            exit 1
        fi
        
        # Copy service file
        cp "$SERVICE_FILE" /etc/systemd/system/
        echo -e "${GREEN}✓ Service file copied${NC}"
        
        # Reload systemd
        systemctl daemon-reload
        echo -e "${GREEN}✓ Systemd reloaded${NC}"
        
        # Enable service
        systemctl enable $SERVICE_NAME
        echo -e "${GREEN}✓ Service enabled (will start on boot)${NC}"
        
        # Start service
        systemctl start $SERVICE_NAME
        echo -e "${GREEN}✓ Service started${NC}"
        
        echo ""
        echo -e "${GREEN}Installation complete!${NC}"
        echo "Check status with: $0 status"
        echo "View logs with: $0 logs"
        ;;
        
    uninstall)
        check_root "$1"
        echo -e "${BLUE}Uninstalling TV LED Monitor service...${NC}"
        
        # Stop service
        systemctl stop $SERVICE_NAME 2>/dev/null
        echo -e "${GREEN}✓ Service stopped${NC}"
        
        # Disable service
        systemctl disable $SERVICE_NAME 2>/dev/null
        echo -e "${GREEN}✓ Service disabled${NC}"
        
        # Remove service file
        rm -f /etc/systemd/system/$SERVICE_NAME
        echo -e "${GREEN}✓ Service file removed${NC}"
        
        # Reload systemd
        systemctl daemon-reload
        echo -e "${GREEN}✓ Systemd reloaded${NC}"
        
        echo ""
        echo -e "${GREEN}Uninstallation complete!${NC}"
        ;;
        
    start)
        check_root "$1"
        echo -e "${BLUE}Starting service...${NC}"
        systemctl start $SERVICE_NAME
        sleep 1
        systemctl status $SERVICE_NAME --no-pager
        ;;
        
    stop)
        check_root "$1"
        echo -e "${BLUE}Stopping service...${NC}"
        systemctl stop $SERVICE_NAME
        echo -e "${GREEN}✓ Service stopped${NC}"
        ;;
        
    restart)
        check_root "$1"
        echo -e "${BLUE}Restarting service...${NC}"
        systemctl restart $SERVICE_NAME
        sleep 1
        systemctl status $SERVICE_NAME --no-pager
        ;;
        
    status)
        systemctl status $SERVICE_NAME --no-pager
        ;;
        
    logs)
        echo -e "${BLUE}Showing live logs (Press Ctrl+C to exit)...${NC}"
        echo ""
        journalctl -u $SERVICE_NAME -f
        ;;
        
    enable)
        check_root "$1"
        echo -e "${BLUE}Enabling service to start on boot...${NC}"
        systemctl enable $SERVICE_NAME
        echo -e "${GREEN}✓ Service enabled${NC}"
        ;;
        
    disable)
        check_root "$1"
        echo -e "${BLUE}Disabling service from starting on boot...${NC}"
        systemctl disable $SERVICE_NAME
        echo -e "${GREEN}✓ Service disabled${NC}"
        ;;
        
    check)
        echo -e "${BLUE}Checking service status...${NC}"
        echo ""
        
        # Check if installed
        if [ -f "/etc/systemd/system/$SERVICE_NAME" ]; then
            echo -e "${GREEN}✓ Service is installed${NC}"
        else
            echo -e "${RED}✗ Service is NOT installed${NC}"
            echo "  Run: sudo $0 install"
            exit 1
        fi
        
        # Check if enabled
        if systemctl is-enabled $SERVICE_NAME >/dev/null 2>&1; then
            echo -e "${GREEN}✓ Service is enabled (will start on boot)${NC}"
        else
            echo -e "${YELLOW}⚠ Service is NOT enabled${NC}"
            echo "  Run: sudo $0 enable"
        fi
        
        # Check if running
        if systemctl is-active $SERVICE_NAME >/dev/null 2>&1; then
            echo -e "${GREEN}✓ Service is running${NC}"
        else
            echo -e "${RED}✗ Service is NOT running${NC}"
            echo "  Run: sudo $0 start"
        fi
        
        echo ""
        echo "Recent logs:"
        journalctl -u $SERVICE_NAME -n 5 --no-pager
        ;;
        
    *)
        show_usage
        exit 1
        ;;
esac

exit 0
