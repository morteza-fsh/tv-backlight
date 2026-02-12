#!/bin/bash
#
# Simple build script for Raspberry Pi (run on the Pi).
# Run setup_pi.sh first to install dependencies.
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build"

echo "Building in ${BUILD_DIR}..."

mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"

cmake -DCMAKE_BUILD_TYPE=Release ..
make -j"$(nproc 2>/dev/null || echo 2)"

echo ""
echo "Done. Binary: ${BUILD_DIR}/bin/app"
