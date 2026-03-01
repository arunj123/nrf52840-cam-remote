#!/bin/bash
# setup_env.sh — Bootstrap the Zephyr development environment for the nRF52840 Camera Remote.
#
# This script:
#   1. Installs required system packages (cmake, ninja, python3-venv, etc.)
#   2. Creates a Python venv and installs west (Zephyr meta-tool)
#   3. Initializes the Zephyr workspace and downloads all modules
#   4. Downloads the Zephyr SDK 0.17.0 (arm-zephyr-eabi toolchain)
#   5. Installs Zephyr's Python dependencies
#
# Usage:
#   chmod +x setup_env.sh
#   ./setup_env.sh              # Interactive (prompts for sudo)
#   ./setup_env.sh --ci         # CI mode (assumes deps are pre-installed, skips sudo)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Source machine-specific overrides if present
if [[ -f "${SCRIPT_DIR}/build.env" ]]; then
    source "${SCRIPT_DIR}/build.env"
fi

# --- Configuration (defaults are relative to the project) ---
ZEPHYR_WORKSPACE="${ZEPHYR_WORKSPACE:-$(dirname "${SCRIPT_DIR}")/zephyrproject}"
ZEPHYR_SDK_VERSION="0.17.0"
ZEPHYR_SDK_DIR="${ZEPHYR_SDK_DIR:-$(dirname "${SCRIPT_DIR}")/zephyr-sdk-${ZEPHYR_SDK_VERSION}}"
CI_MODE=false

if [[ "${1:-}" == "--ci" ]]; then
    CI_MODE=true
fi

info()  { echo -e "\033[1;34m[INFO]\033[0m $*"; }
ok()    { echo -e "\033[1;32m[OK]\033[0m   $*"; }
err()   { echo -e "\033[1;31m[ERR]\033[0m  $*" >&2; }

# --- Step 1: System Dependencies ---
install_system_deps() {
    info "Installing system dependencies..."
    local pkgs=(
        git cmake ninja-build gperf ccache dfu-util device-tree-compiler
        wget python3-dev python3-pip python3-venv python3-setuptools
        xz-utils file make gcc gcc-multilib g++-multilib
        libsdl2-dev libmagic1
    )
    if $CI_MODE; then
        sudo apt-get update -qq
        sudo apt-get install -y -qq "${pkgs[@]}" 2>/dev/null
    else
        sudo apt-get update
        sudo apt-get install -y "${pkgs[@]}"
    fi
    ok "System dependencies installed."
}

# --- Step 2: Python venv + West ---
setup_venv_and_west() {
    if [[ -d "${ZEPHYR_WORKSPACE}/.venv" ]]; then
        info "Existing venv found at ${ZEPHYR_WORKSPACE}/.venv — reusing."
    else
        info "Creating Python virtual environment..."
        mkdir -p "${ZEPHYR_WORKSPACE}"
        python3 -m venv "${ZEPHYR_WORKSPACE}/.venv"
    fi

    source "${ZEPHYR_WORKSPACE}/.venv/bin/activate"
    pip install --quiet --upgrade pip
    pip install --quiet west
    ok "West $(west --version) installed in venv."
}

# --- Step 3: Zephyr Workspace ---
init_zephyr_workspace() {
    if [[ -d "${ZEPHYR_WORKSPACE}/.west" ]]; then
        info "Zephyr workspace already initialized at ${ZEPHYR_WORKSPACE} — updating."
        cd "${ZEPHYR_WORKSPACE}"
        west update
    else
        info "Initializing Zephyr workspace at ${ZEPHYR_WORKSPACE}..."
        mkdir -p "${ZEPHYR_WORKSPACE}"
        cd "${ZEPHYR_WORKSPACE}"
        west init
        west update
    fi
    west zephyr-export
    ok "Zephyr workspace ready. Modules updated."
}

# --- Step 4: Zephyr SDK ---
install_sdk() {
    if [[ -d "${ZEPHYR_SDK_DIR}" ]]; then
        info "Zephyr SDK ${ZEPHYR_SDK_VERSION} already installed at ${ZEPHYR_SDK_DIR} — skipping."
        ok "SDK present."
        return
    fi

    local sdk_archive="zephyr-sdk-${ZEPHYR_SDK_VERSION}_linux-x86_64.tar.xz"
    local sdk_url="https://github.com/zephyrproject-rtos/sdk-ng/releases/download/v${ZEPHYR_SDK_VERSION}/${sdk_archive}"

    info "Downloading Zephyr SDK ${ZEPHYR_SDK_VERSION}..."
    cd "$(dirname "${ZEPHYR_SDK_DIR}")"
    wget -q --show-progress "${sdk_url}"

    info "Extracting SDK..."
    tar -xf "${sdk_archive}"
    rm -f "${sdk_archive}"

    info "Running SDK setup (arm-zephyr-eabi only)..."
    cd "${ZEPHYR_SDK_DIR}"
    ./setup.sh -t arm-zephyr-eabi -c
    ok "Zephyr SDK ${ZEPHYR_SDK_VERSION} installed."
}

# --- Step 5: Python Requirements ---
install_python_deps() {
    info "Installing Zephyr Python requirements..."
    pip install --quiet -r "${ZEPHYR_WORKSPACE}/zephyr/scripts/requirements.txt"
    ok "Python dependencies installed."
}

# --- Main ---
main() {
    echo ""
    echo "=========================================="
    echo "  nRF52840 Camera Remote — Environment Setup"
    echo "=========================================="
    echo ""

    install_system_deps
    setup_venv_and_west
    init_zephyr_workspace
    install_sdk
    install_python_deps

    echo ""
    echo "=========================================="
    ok "Setup complete!"
    echo ""
    echo "  Workspace : ${ZEPHYR_WORKSPACE}"
    echo "  SDK       : ${ZEPHYR_SDK_DIR}"
    echo "  Venv      : ${ZEPHYR_WORKSPACE}/.venv"
    echo ""
    echo "  To build:"
    echo "    cd ${SCRIPT_DIR} && ./build.sh"
    echo "=========================================="
}

main
