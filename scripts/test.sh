#!/bin/bash
# test.sh — Build and run host-based unit tests.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(dirname "${SCRIPT_DIR}")"
BUILD_DIR="${ROOT_DIR}/firmware/build_host"
TEST_DIR="${ROOT_DIR}/firmware/tests"

echo "Building and running unit tests..."

mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"

cmake "${TEST_DIR}"
make -j$(nproc)

echo "------------------------------------------------------------"
./test_gesture_engine
echo "------------------------------------------------------------"
