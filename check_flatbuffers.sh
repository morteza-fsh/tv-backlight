#!/bin/bash
#
# FlatBuffers Installation Diagnostic Script
# Run this on Raspberry Pi to check FlatBuffers installation
#

echo "========================================"
echo "  FlatBuffers Installation Check"
echo "========================================"
echo ""

# Color codes
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Check flatc compiler
echo "1. Checking flatc compiler..."
if command -v flatc &> /dev/null; then
    FLATC_PATH=$(which flatc)
    FLATC_VERSION=$(flatc --version 2>&1 || echo "unknown")
    echo -e "${GREEN}✓${NC} flatc found: $FLATC_PATH"
    echo "  Version: $FLATC_VERSION"
else
    echo -e "${RED}✗${NC} flatc not found"
    echo "  Install: sudo apt install flatbuffers-compiler"
fi
echo ""

# Check include files
echo "2. Checking FlatBuffers headers..."
HEADER_LOCATIONS=(
    "/usr/include/flatbuffers/flatbuffers.h"
    "/usr/local/include/flatbuffers/flatbuffers.h"
    "/opt/homebrew/include/flatbuffers/flatbuffers.h"
    "/opt/homebrew/Cellar/flatbuffers/*/include/flatbuffers/flatbuffers.h"
)

FOUND_HEADER=false
for header in "${HEADER_LOCATIONS[@]}"; do
    if [ -f "$header" ]; then
        echo -e "${GREEN}✓${NC} Header found: $header"
        FOUND_HEADER=true
        break
    fi
done

if [ "$FOUND_HEADER" = false ]; then
    echo -e "${RED}✗${NC} flatbuffers.h not found"
    echo "  Install: sudo apt install libflatbuffers-dev"
fi
echo ""

# Check library files
echo "3. Checking FlatBuffers library..."
LIB_LOCATIONS=(
    "/usr/lib/libflatbuffers.a"
    "/usr/lib/aarch64-linux-gnu/libflatbuffers.a"
    "/usr/lib/arm-linux-gnueabihf/libflatbuffers.a"
    "/usr/lib/x86_64-linux-gnu/libflatbuffers.a"
    "/usr/local/lib/libflatbuffers.a"
    "/opt/homebrew/lib/libflatbuffers.a"
    "/opt/homebrew/Cellar/flatbuffers/*/lib/libflatbuffers.a"
)

FOUND_LIB=false
for lib in "${LIB_LOCATIONS[@]}"; do
    if [ -f "$lib" ]; then
        echo -e "${GREEN}✓${NC} Library found: $lib"
        FOUND_LIB=true
        break
    fi
done

if [ "$FOUND_LIB" = false ]; then
    echo -e "${RED}✗${NC} libflatbuffers.a not found"
    echo "  Install: sudo apt install libflatbuffers-dev"
fi
echo ""

# Check CMake package config
echo "4. Checking CMake package config..."
CMAKE_LOCATIONS=(
    "/usr/lib/cmake/flatbuffers"
    "/usr/local/lib/cmake/flatbuffers"
    "/usr/share/cmake/flatbuffers"
)

FOUND_CMAKE=false
for cmake_dir in "${CMAKE_LOCATIONS[@]}"; do
    if [ -d "$cmake_dir" ]; then
        echo -e "${GREEN}✓${NC} CMake config found: $cmake_dir"
        FOUND_CMAKE=true
        break
    fi
done

if [ "$FOUND_CMAKE" = false ]; then
    echo -e "${YELLOW}⚠${NC} CMake package config not found (this is OK, will use manual detection)"
fi
echo ""

# Summary
echo "========================================"
echo "  Summary"
echo "========================================"

ALL_OK=true
if command -v flatc &> /dev/null; then
    echo -e "${GREEN}✓${NC} flatc compiler: OK"
else
    echo -e "${RED}✗${NC} flatc compiler: MISSING"
    ALL_OK=false
fi

if [ "$FOUND_HEADER" = true ]; then
    echo -e "${GREEN}✓${NC} Headers: OK"
else
    echo -e "${RED}✗${NC} Headers: MISSING"
    ALL_OK=false
fi

if [ "$FOUND_LIB" = true ]; then
    echo -e "${GREEN}✓${NC} Library: OK"
else
    echo -e "${RED}✗${NC} Library: MISSING"
    ALL_OK=false
fi

echo ""

if [ "$ALL_OK" = true ]; then
    echo -e "${GREEN}All required FlatBuffers components are installed!${NC}"
    echo "You should be able to build the project."
    echo ""
    echo "Run:"
    echo "  mkdir build && cd build"
    echo "  cmake .."
    echo "  make"
else
    echo -e "${RED}Some FlatBuffers components are missing!${NC}"
    echo ""
    echo "To install all required components:"
    echo -e "${YELLOW}  sudo apt update${NC}"
    echo -e "${YELLOW}  sudo apt install flatbuffers-compiler libflatbuffers-dev${NC}"
fi
echo ""

