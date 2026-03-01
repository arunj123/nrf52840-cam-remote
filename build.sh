#!/bin/bash
# build.sh — Build the nRF52840 Camera Remote firmware.
#
# Usage:
#   ./build.sh              # Standard build
#   ./build.sh --pristine   # Clean rebuild
#   ./build.sh --flash      # Build then copy hex to Windows for flashing

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Source machine-specific overrides if present
if [[ -f "${SCRIPT_DIR}/build.env" ]]; then
    source "${SCRIPT_DIR}/build.env"
fi

APP_DIR="${SCRIPT_DIR}/app"
BUILD_DIR="${APP_DIR}/build"
BOARD="nrf52840_mdk"

# Defaults are relative to the project. Override in build.env.
ZEPHYR_WORKSPACE="${ZEPHYR_WORKSPACE:-$(dirname "${SCRIPT_DIR}")/zephyrproject}"
VENV_DIR="${ZEPHYR_WORKSPACE}/.venv"

PRISTINE=""
DO_FLASH=false

for arg in "$@"; do
    case "$arg" in
        --pristine) PRISTINE="-p" ;;
        --flash)    DO_FLASH=true ;;
        *)          echo "Unknown argument: $arg"; exit 1 ;;
    esac
done

# Activate the Zephyr venv
if [[ ! -d "${VENV_DIR}" ]]; then
    echo "Error: Zephyr venv not found at ${VENV_DIR}."
    echo "Run ./setup_env.sh first."
    exit 1
fi
source "${VENV_DIR}/bin/activate"

echo "Building firmware for ${BOARD}..."
cd "${ZEPHYR_WORKSPACE}"
west build ${PRISTINE} -b "${BOARD}" -d "${BUILD_DIR}" "${APP_DIR}" 2>&1

HEX_FILE="${BUILD_DIR}/zephyr/zephyr.hex"
if [[ ! -f "${HEX_FILE}" ]]; then
    echo "Error: Build did not produce ${HEX_FILE}"
    exit 1
fi

echo ""
echo "Build successful: ${HEX_FILE}"

# Optional: copy to Windows for flashing via SWD
if $DO_FLASH; then
    WINDOWS_FLASH_DIR="${WINDOWS_FLASH_DIR:-/mnt/c/nrf_recovery}"
    OPENOCD_DIR="${OPENOCD_DIR:-C:/Users/<user>/.pico-sdk/openocd/0.12.0+dev}"

    mkdir -p "${WINDOWS_FLASH_DIR}"
    cp "${HEX_FILE}" "${WINDOWS_FLASH_DIR}/app.hex"

    # Also copy the OpenOCD config if present
    if [[ -f "${SCRIPT_DIR}/nrf52_stlink.cfg" ]]; then
        cp "${SCRIPT_DIR}/nrf52_stlink.cfg" "${WINDOWS_FLASH_DIR}/nrf52_stlink.cfg"
    fi

    echo "Copied to ${WINDOWS_FLASH_DIR}/app.hex"
    echo ""
    echo "Flash from PowerShell:"
    echo "  & \"${OPENOCD_DIR}/openocd.exe\" \`"
    echo "    -s \"${OPENOCD_DIR}/scripts\" \`"
    echo "    -f \"C:/nrf_recovery/nrf52_stlink.cfg\" \`"
    echo '    -c "init; halt; nrf5 mass_erase; reset halt; flash write_image C:/nrf_recovery/app.hex; reset; exit"'
fi
