# nRF52840 BLE Camera Remote

A Bluetooth Low Energy HID Camera Remote using the MakerDiary nRF52840 MDK USB Dongle and Zephyr RTOS. Triggers a smartphone camera shutter (via Volume Up HID command) using a physical button or by shorting GPIO pins.

## 1. Hardware Setup

### Supported Boards
- **Pro Micro nRF52840**: A Pro Micro-compatible nRF52840 board (e.g., Nice!Nano, Bluemicro, or custom).

### Pin Mappings (Pro Micro)

The firmware is configured for the following physical connections:

| Physical Pin # | Label | GPIO | Function in App |
| :--- | :--- | :--- | :--- |
| **Pin 5** | **D2** | `P0.10` | Encoder Phase A |
| **Pin 6** | **D3** | `P1.11` | Encoder Phase B |
| **Pin 7** | **D4** | `P0.17` | **Trigger 2** |
| **Pin 8** | **D5** | `P0.20` | **Trigger 1** (Main Shutter) |
| **Pin 9** | **D6** | `P0.22` | Buzzer (4kHz PWM) |
| **Pin 10** | **D7** | `P0.24` | **Encoder Switch** (Mute/Profile) |
| **On-board** | **LED** | `P0.15` | Status Indicator (Blue/Red) |

---

### Hardware Verification
If you are setting up new hardware, you can use the **`board-verification`** branch to test your wiring (LED blinking, buzzer beeps, and USB encoder logs).

```bash
git checkout board-verification
```

### LED & Audio Feedback
- **🔵 Blue**: Advertising (Looking for a pair).
- **🟢 Green**: Connected (Ready to use).
- **🔴 Red + 🔊 Beep**: Trigger detected.

> **Note**: To maximize battery life, LEDs are automatically disabled during idle sleep while connected or advertising. A **passive ceramic piezo buzzer** is used for audio feedback (connected between P2 and GND, driven by 4kHz PWM).

### Click Modes
| Gesture | Device | Action | Buzzer Feedback |
|---------|--------|--------|------------------|
| **Press** | Main Button | Photo / Video Start-Stop (Volume Up) | 1 short beep |
| **Hold >0.8s** | Main Button | Burst Mode (hold Volume Up) | 1 long beep |
| **Rotate C/W** | Encoder | Volume Up | None |
| **Rotate CC/W** | Encoder | Volume Down | None |
| **Press** | Encoder | Mute Toggle | None |
| **Hold >0.8s** | Encoder | Cycle Profile Slot (1→2→3→1) | N beeps |

> **iPhone**: On iPhone 11+, holding Volume Up triggers QuickTake by default. To enable burst: **Settings > Camera > Use Volume Up for Burst**.

### Battery Monitoring
The firmware reports battery level to your phone via BLE Battery Service (BAS).
- **No external wiring needed** — uses the nRF52840's internal SAADC to measure VDD.
- **To power from battery**: Connect a LiPo (3.7V) to **VIN** (+) and **GND** (-) on the dongle.
- Battery level updates every 60 seconds and appears in your phone's Bluetooth settings.

### Power Management
- **System ON Idle**: CPU enters low-power idle (WFE/WFI) when waiting for button events.
- **Interrupt-driven**: The button engine uses GPIO interrupts to wake the system from sleep, ensuring zero power waste during polling.
- **DC/DC mode**: Enabled for lower quiescent current.
- **LED Dimming**: Status LEDs are switched off during sleep states to prolong battery life.

---

## 2. Software & Development

### Architecture (Modern C++ Refactor)
The project is built using **Modern C++23** for improved modularity and safety.
- **OS**: Zephyr RTOS v4.x
- **Standard**: C++23 (using `std::span`, `std::array`, and structured classes)
- **Logic**: Encapsulated in `remote::HidService`, `remote::BluetoothManager`, `remote::LedController`, `remote::BuzzerController`, and `remote::BatteryMonitor`.
- **Gesture Logic**: Detailed state transitions and timing are documented in [docs/gesture_logic.md](docs/gesture_logic.md).
- **Button Engine**: Interrupt-driven thread with automated power-saving sleep. Deadlock-proof.
- **Profile**: Consumer Control (Media Remote) via HOGP.
- **Pairing**: Passkey (MITM) pairing to satisfy Windows 10/11 security enforcement for BLE Keyboards.

### Developer Rules & Requirements
To maintain consistency and high code quality, this project uses automated agent workflows:
- **[Functional Requirements](.agent/workflows/functional-requirements.md)**: Defines the project scope, feature set, and non-functional constraints.
- **[Technical Standards & Rules](.agent/workflows/technical-standards.md)**: Defines coding standards (C++23), TDD requirements, and documentation rules.

**Rule**: These documents **MUST** be updated whenever functional or technical changes are made.

### Directory Structure
```text
nrf_control/
├── firmware/     # Zephyr C++/DTS source code & build config
├── scripts/      # Bootstrap, build, and environment scripts
├── hardware/     # 3D models (STL/STEP) for the case
├── docs/         # Wiring diagrams and documentation
└── README.md     # You are here
```

### Quick Start

**Prerequisites**: Ubuntu/WSL with `git`, `cmake`, `python3`.

```bash
# 1. Clone the project
git clone <repo-url> && cd nrf_control

# 2. Configure machine-specific paths
cp scripts/build.env.example scripts/build.env
# Edit scripts/build.env with your OpenOCD path, etc.

# 3. Bootstrap the Zephyr toolchain (one-time setup, ~10 minutes)
./scripts/setup_env.sh

# 4. Build the firmware (MDK Dongle by default)
./scripts/build.sh

# 5. Build for Pro Micro nRF52840 (generates UF2)
./scripts/build.sh --board promicro_nrf52840
# Result: firmware/build_promicro_nrf52840_nrf52840_uf2/zephyr/zephyr.uf2

# 6. Build + copy hex to Windows for flashing
./scripts/build.sh --flash
```

### Build Scripts

| Script | Purpose |
|--------|---------|
| `scripts/setup_env.sh` | Installs system deps, creates Python venv, initializes Zephyr workspace, downloads SDK 0.17.0 |
| `scripts/setup_env.sh --ci` | Same as above but quieter output for CI |
| `scripts/build.sh` | Activates venv and runs `west build` |
| `scripts/build.sh --board <name>` | Select board target (`nrf52840_mdk` or `promicro_nrf52840`) |
| `scripts/build.sh --pristine` | Clean rebuild (wipes build directory first) |
| `scripts/build.sh --flash` | Builds and copies `zephyr.hex` to Windows flash directory |

### Configuration (`build.env`)

Machine-specific paths live in `scripts/build.env` (gitignored). Copy the template and edit:
```bash
cp scripts/build.env.example scripts/build.env
```

Available settings:
| Variable | Default | Description |
|----------|---------|-------------|
| `ZEPHYR_WORKSPACE` | `../zephyrproject` | Zephyr workspace directory |
| `ZEPHYR_SDK_DIR` | `../zephyr-sdk-0.17.0` | Zephyr SDK installation |
| `OPENOCD_DIR` | *(none)* | Windows OpenOCD path for flashing |
| `WINDOWS_FLASH_DIR` | `/mnt/c/nrf_recovery` | Where to copy hex for Windows flash |

### Manual Build (Advanced)
If you prefer not to use the wrapper scripts:
```bash
source <ZEPHYR_WORKSPACE>/.venv/bin/activate
cd <ZEPHYR_WORKSPACE>
# For MDK Dongle
west build -b nrf52840_mdk -d <project>/firmware/build_nrf52840_mdk <project>/firmware
# For Pro Micro
west build -b promicro_nrf52840 -d <project>/firmware/build_promicro_nrf52840 <project>/firmware
```

---

## 3. Flashing Process

Firmware is built in WSL and flashed from Windows using OpenOCD via ST-Link V2.

### Step 1: Copy Hex to Windows
```bash
./scripts/build.sh --flash
```
This copies `firmware/build/zephyr/zephyr.hex` → `C:/nrf_recovery/app.hex`.

### Step 2: Flash from PowerShell
Set `$OPENOCD_DIR` to your OpenOCD installation (or configure it in `build.env`):
```powershell
$OPENOCD_DIR = "C:/Users/<your-user>/.pico-sdk/openocd/0.12.0+dev"
& "$OPENOCD_DIR/openocd.exe" `
  -s "$OPENOCD_DIR/scripts" `
  -f "C:/nrf_recovery/nrf52_stlink.cfg" `
  -c "init; halt; nrf5 mass_erase; reset halt; flash write_image C:/nrf_recovery/app.hex; reset; exit"
```

### Step 3: Monitor Logs from Windows
To view debug output (printk) from the device, use `pyserial`'s miniterm in PowerShell:

1. **Identify COM Port**: Plug in your serial adapter (connected to pins P0.20/P0.19) and find the COM port in Device Manager.
2. **Run Monitor**:
```powershell
pip install pyserial
python -m serial.tools.miniterm COM<X> 115200
```
> **Note**: Replace `COM<X>` with your actual port (e.g., `COM3`). Exit with `Ctrl+]`.

### Step 4: Pro Micro Flashing (UF2)
1. **Enter Bootloader**: Quickly **double-tap** the Reset button (or bridge RST to GND twice).
2. **Mount**: A new drive named `UF2BOOT` will appear on your computer.
3. **Flash**: Drag and drop the `zephyr.uf2` file from `firmware/build_promicro_nrf52840_nrf52840_uf2/zephyr/` onto the `UF2BOOT` drive.
4. The board will automatically reboot with the new firmware.

---

### Step 5: Monitor Logs (Windows/USB)
The Pro Micro port is configured as a USB CDC ACM console. Use `pyserial`'s miniterm:

1. **Identify COM Port**: Check Device Manager for "Zephyr CDC ACM" (e.g., COM12).
2. **Run Monitor**:
```powershell
python -m serial.tools.miniterm COM<X> 115200
```

### Step 5: Pair with Phone
1. On Android/iOS: **Settings → Bluetooth → Scan**.
2. Pair with **"Cam Remote Pro"**.
3. Open the Camera app and test the trigger.

> **Note**: If you re-flash via SWD (ST-Link), you must **forget/unpair** the device on your phone first. UF2 flashing usually preserves bonding if the bootloader supports it, but its safer to re-pair if connection fails.

---

## 4. CI / GitHub Actions

The project includes a GitHub Actions workflow (`.github/workflows/build.yml`) that:
- Caches the **Zephyr SDK** (~1.5 GB) and **west modules** between runs.
- Builds the firmware automatically on push/PR to the **`master`** branch.
- Uploads `zephyr.hex` as a downloadable GitHub Actions artifact.

---

## 5. Bluetooth Details
- **Device Name**: `Cam Remote Pro`
- **Appearance**: Keyboard (961) - Required for Windows to correctly map Consumer Control buttons natively.
- **PnP ID**: Vendor `0x05AC`, Product `0x0220` (Apple Vendor ID used for driver-less Windows compatibility)
- **Security**: Passkey/MITM encryption (BT_SECURITY_L4). Windows 10/11 strictly mandates Passkey pairing for BLE Keyboards to prevent injection attacks, so a 6-digit PIN is displayed on the serial console during pairing.
- **Advertising Watchdog**: Background worker ensures advertising automatically restarts if the BLE stack hits an error or if the device disconnects.
- **Persistent Bonding**: Pairing info is stored in NVS, allowing auto-reconnection after power cycles.
- **Multi-Device Profiles**: Supports bonding with up to 3 devices. One active connection at a time; cycle with encoder button long-press.
- **Provisioning Service**: Custom GATT service skeleton (vendor UUID) for future button-function programming via companion app.

---

## 6. Hardware & 3D Modeling (Coming Soon)


---

## 7. Unit Testing

The project uses **GoogleTest** for host-based unit testing of the core logic (e.g., `GestureEngine`). These tests run on your development machine (Linux/WSL) and do not require the nRF52 hardware.

### Running Tests
Use the provided helper script to build and run all tests:
```bash
./scripts/test.sh
```

### Test Directory Structure
- `firmware/tests/`: Contains the test source code and `CMakeLists.txt`.
- `firmware/build_host/`: Where the host-based test binaries are built.
