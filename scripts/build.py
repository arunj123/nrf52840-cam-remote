#!/usr/bin/env python3
"""
Windows / Cross-platform Build, Unit-Test & UF2 Flash Script for nRF52840 Camera Remote.

- Host unit tests (`--test`): configures & runs GoogleTest suite in firmware/build_host_win (or build_host).
- Firmware build: detects installed nRF Connect SDK (C:\\ncs\\v*) + toolchain, compiles firmware/
  for adafruit_feather_nrf52840/nrf52840/uf2 (Nice!Nano / Pro Micro nRF52840 compatible), prints Flash/RAM usage.
- UF2 flashing (`--flash`): writes zephyr.uf2 to mounted NICENANO / UF2 drive, verifies reboot,
  and automatically falls back to adafruit-nrfutil Serial DFU if the drive stays mounted.
"""

import argparse
import glob
import os
import shutil
import subprocess
import sys
import time

SCRIPT_DIR = os.path.abspath(os.path.dirname(__file__))
REPO_ROOT = os.path.abspath(os.path.join(SCRIPT_DIR, ".."))
APP_DIR = os.path.join(REPO_ROOT, "firmware")
TESTS_DIR = os.path.join(APP_DIR, "tests")

TC_BASE = r"C:\ncs\toolchains\c57af46cb7"
SIZE_TOOL = os.path.join(TC_BASE, "opt", "zephyr-sdk", "arm-zephyr-eabi", "bin", "arm-zephyr-eabi-size.exe")

FLASH_MAX_BYTES = 1024 * 1024  # 1 MB Flash on nRF52840
RAM_MAX_BYTES = 256 * 1024     # 256 KB SRAM on nRF52840


def detect_ncs_base() -> str:
    candidates = sorted(glob.glob(r"C:\ncs\v*"), reverse=True)
    for c in candidates:
        if os.path.exists(os.path.join(c, "zephyr")):
            return c
    return r"C:\ncs\v2.8.0"


def find_cached_googletest() -> str | None:
    candidates = [
        os.path.join(APP_DIR, "build_host", "_deps", "googletest-src"),
        os.path.join(TESTS_DIR, "build_host", "_deps", "googletest-src"),
        os.path.join(APP_DIR, "build_host_win", "_deps", "googletest-src"),
    ]
    for path in candidates:
        if os.path.isfile(os.path.join(path, "CMakeLists.txt")):
            return path.replace("\\", "/")
    return None


def run_host_tests(clean: bool = False) -> bool:
    print("\n==================================================")
    print(" Running Host Unit Tests (GoogleTest)")
    print("==================================================")

    build_dir = os.path.join(APP_DIR, "build_host_win" if os.name == "nt" else "build_host")
    if clean and os.path.exists(build_dir):
        shutil.rmtree(build_dir, ignore_errors=True)
    os.makedirs(build_dir, exist_ok=True)

    cmake_args = ["cmake", "-S", TESTS_DIR, "-B", build_dir]
    gtest_src = find_cached_googletest()
    if gtest_src:
        print(f"Using cached GoogleTest source: {gtest_src}")
        cmake_args.append(f"-DFETCHCONTENT_SOURCE_DIR_GOOGLETEST={gtest_src}")

    res = subprocess.run(cmake_args, cwd=REPO_ROOT)
    if res.returncode != 0:
        print("[ERROR] CMake configure failed for host unit tests.")
        return False

    build_cmd = ["cmake", "--build", build_dir, "--parallel"]
    if os.name == "nt":
        build_cmd.extend(["--config", "Release"])

    res = subprocess.run(build_cmd, cwd=REPO_ROOT)
    if res.returncode != 0:
        print("[ERROR] Host unit test build failed.")
        return False

    ctest_cmd = ["ctest", "--test-dir", build_dir, "--output-on-failure"]
    if os.name == "nt":
        ctest_cmd.extend(["-C", "Release"])

    res = subprocess.run(ctest_cmd, cwd=REPO_ROOT)
    return res.returncode == 0


def print_memory_report(target_name: str, elf_path: str) -> None:
    if not os.path.exists(elf_path) or not os.path.exists(SIZE_TOOL):
        return

    try:
        res = subprocess.run([SIZE_TOOL, elf_path], capture_output=True, text=True, check=True)
        lines = res.stdout.strip().splitlines()
        if len(lines) >= 2:
            parts = lines[1].split()
            text_size = int(parts[0])
            data_size = int(parts[1])
            bss_size = int(parts[2])

            flash_used = text_size + data_size
            ram_used = data_size + bss_size

            flash_pct = (flash_used / FLASH_MAX_BYTES) * 100.0
            ram_pct = (ram_used / RAM_MAX_BYTES) * 100.0

            print("\n--------------------------------------------------")
            print(f" [MEMORY USAGE SUMMARY: {target_name.upper()}]")
            print("--------------------------------------------------")
            print(f"  ROM / Flash Used : {flash_used:>8,d} Bytes / {FLASH_MAX_BYTES:,d} Bytes ({flash_pct:5.2f}%)")
            print(f"  RAM Used         : {ram_used:>8,d} Bytes / {RAM_MAX_BYTES:,d} Bytes ({ram_pct:5.2f}%)")
            print(f"  Flash Free       : {FLASH_MAX_BYTES - flash_used:>8,d} Bytes")
            print(f"  RAM Free         : {RAM_MAX_BYTES - ram_used:>8,d} Bytes")
            print("--------------------------------------------------\n")
    except Exception as exc:
        print(f"Could not calculate memory report: {exc}")


def find_uf2_drive() -> str | None:
    for letter in "DEFGHIJKLMNOPQRSTUVWXYZ":
        drive = f"{letter}:\\"
        if os.path.exists(os.path.join(drive, "INFO_UF2.TXT")):
            return drive
    return None


def find_bootloader_com_port() -> str | None:
    try:
        import serial.tools.list_ports

        for port in serial.tools.list_ports.comports():
            if port.vid == 0x239A and port.pid == 0x00B3:
                return port.device
    except Exception:
        pass
    return None


def flash_serial_dfu(hex_path: str, build_dir: str, port: str | None = None) -> bool:
    if not os.path.exists(hex_path):
        print(f"[ERROR] HEX binary not found for Serial DFU fallback: {hex_path}")
        return False

    target_port = port or find_bootloader_com_port()
    if not target_port:
        print("[ERROR] Could not detect Adafruit nRF52 bootloader COM port (VID:PID 239A:00B3).")
        return False

    nrfutil_exe = shutil.which("adafruit-nrfutil")
    if not nrfutil_exe:
        print("[ERROR] adafruit-nrfutil not found in PATH.")
        return False

    pkg_path = os.path.join(build_dir, "dfu_package.zip")
    print(f"[DFU] Generating DFU package {pkg_path}...")
    gen = subprocess.run(
        [nrfutil_exe, "dfu", "genpkg", "--dev-type", "0x0052", "--application", hex_path, pkg_path],
        cwd=REPO_ROOT,
    )
    if gen.returncode != 0:
        return False

    print(f"[DFU] Flashing over {target_port} at 115200 baud...")
    flash = subprocess.run(
        [nrfutil_exe, "--verbose", "dfu", "serial", "-pkg", pkg_path, "-p", target_port, "-b", "115200"],
        cwd=REPO_ROOT,
    )
    return flash.returncode == 0


def flash_device(uf2_path: str, hex_path: str, build_dir: str, port: str | None = None) -> bool:
    if not os.path.exists(uf2_path):
        print(f"[ERROR] UF2 binary not found at: {uf2_path}")
        return False

    drive = find_uf2_drive()
    if not drive:
        print("[WARN] UF2 bootloader drive not mounted; attempting Serial DFU...")
        return flash_serial_dfu(hex_path, build_dir, port=port)

    print(f"\n[FLASHING] Found UF2 Drive at {drive}")
    info_path = os.path.join(drive, "INFO_UF2.TXT")
    target_path = os.path.join(drive, "FLASH.UF2")

    with open(uf2_path, "rb") as src:
        data = src.read()

    print(f"  -> Writing {len(data):,d} bytes to {target_path}...")
    try:
        with open(target_path, "wb") as dst:
            dst.write(data)
            dst.flush()
            os.fsync(dst.fileno())
    except OSError as exc:
        # Windows sometimes raises an error as the mass storage device unmounts on reboot
        print(f"  -> UF2 write triggered disconnect ({exc}).")

    # Wait up to 5 seconds to verify the UF2 bootloader unmounted and rebooted
    for _ in range(10):
        time.sleep(0.5)
        if not os.path.exists(info_path):
            print("  -> UF2 bootloader unmounted cleanly. Device rebooted into application.")
            return True

    print("[WARN] UF2 drive still mounted after 5s; falling back to Serial DFU...")
    return flash_serial_dfu(hex_path, build_dir, port=port)


def build_firmware(board: str | None = None, clean: bool = False, flash: bool = False, port: str | None = None) -> bool:
    ncs_base = detect_ncs_base()
    ncs_version = os.path.basename(ncs_base)

    if board is None:
        board = "adafruit_feather_nrf52840/nrf52840/uf2"

    safe_board = board.replace("/", "_")
    build_dir = os.path.join(APP_DIR, f"build_{safe_board}")

    print("\n==================================================")
    print(f" Building Cam Remote Firmware using NCS {ncs_version}")
    print(f" NCS Base  : {ncs_base}")
    print(f" Target    : {board}")
    print(f" Build Dir : {build_dir}")
    print("==================================================")

    if clean and os.path.exists(build_dir):
        print(f"Cleaning build directory: {build_dir}")
        shutil.rmtree(build_dir, ignore_errors=True)

    env = os.environ.copy()
    path_additions = [
        os.path.join(TC_BASE, "mingw64", "bin"),
        os.path.join(TC_BASE, "bin"),
        os.path.join(TC_BASE, "opt", "bin"),
        os.path.join(TC_BASE, "opt", "bin", "Scripts"),
        os.path.join(TC_BASE, "opt", "zephyr-sdk", "arm-zephyr-eabi", "bin"),
    ]
    env["PATH"] = ";".join(path_additions) + ";" + env.get("PATH", "")
    env["PYTHONPATH"] = f"{TC_BASE}\\opt\\bin;{TC_BASE}\\opt\\bin\\Lib;{TC_BASE}\\opt\\bin\\Lib\\site-packages"
    env["ZEPHYR_TOOLCHAIN_VARIANT"] = "zephyr"
    env["ZEPHYR_SDK_INSTALL_DIR"] = os.path.join(TC_BASE, "opt", "zephyr-sdk")
    env["ZEPHYR_BASE"] = os.path.join(ncs_base, "zephyr")

    west_exe = os.path.join(TC_BASE, "opt", "bin", "Scripts", "west.exe")
    if not os.path.exists(west_exe):
        west_exe = "west"

    cmd = [west_exe, "build", "--no-sysbuild", "-b", board, "-d", build_dir, APP_DIR]
    print(f"Executing: {' '.join(cmd)}")
    res = subprocess.run(cmd, cwd=REPO_ROOT, env=env)
    if res.returncode != 0:
        print(f"\n[ERROR] Zephyr build failed with exit code {res.returncode}")
        return False

    elf_path = os.path.join(build_dir, "zephyr", "zephyr.elf")
    hex_path = os.path.join(build_dir, "zephyr", "zephyr.hex")
    uf2_path = os.path.join(build_dir, "zephyr", "zephyr.uf2")

    print("\n[SUCCESS] Firmware build completed!")
    print(f"  -> ELF : {elf_path}")
    print(f"  -> HEX : {hex_path}")
    print(f"  -> UF2 : {uf2_path}")
    print_memory_report(board, elf_path)

    if flash:
        return flash_device(uf2_path, hex_path, build_dir, port=port)

    return True


def main() -> None:
    parser = argparse.ArgumentParser(description="Build, Test & Flash nRF52840 Camera Remote")
    parser.add_argument("--test", action="store_true", help="Build and run host GoogleTest unit tests")
    parser.add_argument("--board", type=str, default=None, help="Zephyr board target")
    parser.add_argument("--clean", action="store_true", help="Perform pristine/clean rebuild")
    parser.add_argument("--flash", action="store_true", help="Flash UF2 firmware to connected bootloader")
    parser.add_argument("--port", type=str, default=None, help="Explicit COM port for Serial DFU fallback")
    args = parser.parse_args()

    if args.test:
        ok = run_host_tests(clean=args.clean)
        sys.exit(0 if ok else 1)

    ok = build_firmware(board=args.board, clean=args.clean, flash=args.flash, port=args.port)
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
