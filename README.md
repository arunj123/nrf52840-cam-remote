# nRF52840 BLE Camera Remote

A Bluetooth Low Energy HID Camera Remote using a Pro Micro nRF52840 / Nice!Nano V2 compatible board and Zephyr RTOS. Triggers a smartphone camera shutter (via Consumer Control Volume Up or custom mapped HID keys) using physical buttons and a rotary encoder.

## 1. Hardware Setup

### Supported Boards
- **Pro Micro nRF52840 / Nice!Nano V2** (or compatible clones such as Tenstar / SuperMini nRF52840).
  - Windows NCS v2.8.0 build target: `adafruit_feather_nrf52840/nrf52840/uf2` (with project board overlay).
  - Linux / WSL / Zephyr v4.3.0 CI build target: `promicro_nrf52840/nrf52840/uf2`.

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
| **Internal** | **VCC Gate** | `P0.13` | External 3.3V Rail Enable (`ACTIVE_HIGH`) |
| **On-board** | **LED** | `P0.15` | Status Indicator (Red) |

---

### Hardware Verification
If you are setting up new hardware, you can use the **`board-verification`** branch to test your wiring (LED blinking, buzzer beeps, and USB encoder logs).

```bash
git checkout board-verification
```

### LED & Audio Feedback
- **🔴 Red LED (`P0.15`) + 🔊 Beep (`D6` / `P0.22`)**: Trigger detected (single click = short beep, burst = long beep).
- **Profile Switch**: `N+1` short beeps when cycling to Slot `N` (Slots `0–2` HID, Slot `3` Config Mode).
- **Bond Clear (5s hold)**: 3 countdown beeps followed by 1 long beep.

> **Note**: A **passive ceramic piezo buzzer** is driven between **D6 (`P0.22`)** and **GND** using 4 kHz PWM (`H0H1` high-drive). On Nice!Nano clones, the blue LED is hardwired to the TP4054 charger IC (`CHRG`), not the MCU.

### Click Modes
| Gesture | Device | Default Action (Custom via `config.html`) | Buzzer Feedback |
|---------|--------|-------------------------------------------|-----------------|
| **Press (<0.8s)** | Main Button | Photo / Video Start-Stop (`Volume Up`) | 1 short beep |
| **Hold (0.8s–5s)** | Main Button | Burst Mode (hold `Volume Up` for 2s) | 1 long beep |
| **Hold (>=5s)** | Main Button | Clear Active Slot Bond & Rotate BT ID | Countdown + long beep |
| **Rotate C/W** | Encoder | `Volume Up` | None |
| **Rotate CC/W** | Encoder | `Volume Down` | None |
| **Press (<0.8s)** | Encoder | `Mute` Toggle | None |
| **Hold (>=0.8s)** | Encoder | Cycle Slot (`0 → 1 → 2 → 3 → 0`) | `N+1` beeps |

> **iPhone**: On iPhone 11+, holding Volume Up triggers QuickTake by default. To enable burst: **Settings > Camera > Use Volume Up for Burst**.

### Battery Monitoring
The firmware reports battery level to your phone via BLE Battery Service (BAS).
- Reads `NRF_SAADC_AIN5` (`P0.29`, 12-bit with 16x hardware oversampling and 32-sample moving average).
- **To power from battery**: Connect a single-cell Li-Po (3.7V) to **BAT/B+** and **GND** (`-`).
- Battery level updates every 60 seconds and appears in your phone's Bluetooth settings.

### Power Management
- **System ON Idle**: CPU enters low-power idle (`WFE/WFI`) when waiting for GPIO or QDEC interrupt events.
- **Interrupt-driven**: Main buttons, encoder button, and hardware QDEC rotation (`qdec_nrfx` trigger deferred to thread) wake the system without polling.
- **LDO Regulator Mode**: Configured for safe operation across clone boards that omit external DC/DC inductors.

---

## 2. Software & Development

### Architecture (Modern C++)
The project is built using **Modern C++** (`C++23` on Zephyr 4.3 / `C++2B` on NCS v2.8.0) for improved modularity and safety.
- **OS**: Zephyr RTOS (`v3.7.99` in NCS v2.8.0 / `v4.3.0` in CI)
- **Standard**: `std::span`, `std::array`, `std::string_view`, `std::optional`, template HAL abstractions (`GestureEngine`, `ProfileManager`)
- **Logic**: Encapsulated in `remote::HidService`, `remote::BluetoothManager`, `remote::LedController`, `remote::BuzzerController`, and `remote::BatteryMonitor`.
- **Gesture Logic**: Detailed state transitions and timing are documented in [docs/gesture_logic.md](docs/gesture_logic.md).
- **Profile**: Consumer Control (Media Remote) via HOGP on Slots `0–2`, plus Web Bluetooth Config Mode (`Cam Config`) on Slot `3` paired with [config.html](config.html).
- **Pairing**: Just Works encryption (`BT_SECURITY_L2`) by default; Passkey/MITM pairing (`BT_SECURITY_L4`) can be enabled with `CONFIG_SAMPLE_BT_USE_AUTHENTICATION=y`.

### Developer Rules, Skills & Roadmap
To maintain consistency and high code quality:
- **[Roadmap & Status](docs/ROADMAP.md)**: Tracks active architecture status, review findings, and session handover notes.
- **[Project Requirements](.agent/rules/requirements.md)**: Defines functional and non-functional constraints.
- **[Coding Standards](.agent/rules/coding-standards.md)** & **[Testing Standards](.agent/rules/testing-standards.md)**: Define C++23 coding rules, hardware abstraction, and TDD requirements.
- **Shared Skills**: Workspace skills live centrally in `../.agents/skills/` (inherited from the parent `projects` workspace).

**Rule**: These documents **MUST** be updated whenever functional or technical changes are made.

### Directory Structure
```text
nrf_control/
├── config.html   # Web Bluetooth configurator for Slot 3 (Cam Config)
├── docs/         # Roadmap, gesture state diagrams, and documentation
├── firmware/     # Zephyr C++/DTS source code, board overlays, & unit tests
├── scripts/      # Windows build/test/flash (build.py) & Linux/WSL scripts
└── README.md     # You are here
```

### Quick Start (Windows — nRF Connect SDK v2.8.0)

```powershell
# 1. Run host unit tests (GoogleTest via CMake + MSVC)
python scripts/build.py --test

# 2. Build UF2 firmware (Nice!Nano / Pro Micro nRF52840)
python scripts/build.py --clean

# 3. Build & flash directly to connected UF2 bootloader (with Serial DFU fallback)
python scripts/build.py --flash
```

### Quick Start (Linux / WSL — Zephyr v4.3.0)

**Prerequisites**: Ubuntu/WSL with `git`, `cmake`, `python3`.

```bash
# 1. Clone the project
git clone <repo-url> && cd nrf_control

# 2. Configure machine-specific paths
cp scripts/build.env.example scripts/build.env

# 3. Bootstrap the Zephyr toolchain (one-time setup, ~10 minutes)
./scripts/setup_env.sh

# 4. Run host unit tests
./scripts/test.sh

# 5. Build for Pro Micro nRF52840 (generates UF2)
./scripts/build.sh --board promicro_nrf52840

# 6. Build + copy hex to Windows for flashing
./scripts/build.sh --flash
```

### Build Scripts

| Script | Purpose |
|--------|---------|
| `scripts/build.py` | Windows NCS v2.8.0 build, GoogleTest runner (`--test`), and UF2/Serial DFU flasher (`--flash`) |
| `scripts/setup_env.sh` | Installs Linux/WSL deps, creates Python venv, initializes Zephyr v4.3.0 workspace, downloads SDK 0.17.0 |
| `scripts/build.sh` | Activates Linux/WSL venv and runs `west build` |
| `scripts/test.sh` | Builds and runs GoogleTest host unit tests on Linux/WSL |

---

## 3. Flashing & Monitoring

### Option A: UF2 / Serial DFU (Pro Micro / Nice!Nano)
1. **Enter Bootloader**: Quickly **double-tap** the Reset button (or short `RST` to `GND` twice) until the `NICENANO` / `UF2BOOT` drive mounts.
2. **Flash**: Run `python scripts/build.py --flash` (writes `zephyr.uf2` and automatically falls back to `adafruit-nrfutil` Serial DFU if Windows write-caching holds the drive).
3. The board automatically reboots into the application.

### Option B: SWD via OpenOCD (ST-Link V2)
1. Copy hex with `./scripts/build.sh --flash` (`C:/nrf_recovery/app.hex`).
2. Flash from PowerShell:
```powershell
$OPENOCD_DIR = "C:/Users/<your-user>/.pico-sdk/openocd/0.12.0+dev"
& "$OPENOCD_DIR/openocd.exe" `
  -s "$OPENOCD_DIR/scripts" `
  -f "C:/nrf_recovery/nrf52_stlink.cfg" `
  -c "init; halt; nrf5 mass_erase; reset halt; flash write_image C:/nrf_recovery/app.hex; reset; exit"
```

### Step 4: Monitor Logs (USB CDC ACM)
The firmware exposes a USB CDC ACM serial console (`VID:PID 239A:00B3` on NCS builds):
```powershell
python -m serial.tools.miniterm COM<X> 115200
```
*(Replace `COM<X>` with your actual port. Exit with `Ctrl+]`).*

### Step 5: Pair & Configure
1. **Pair HID (Slots 0–2)**: On Android/iOS/Windows, scan and pair with **"Cam Remote Pro"**.
2. **Configure Mappings (Slot 3)**: Long-press the encoder button (`>= 0.8s`) to cycle to Slot `3` (4 beeps, advertises **"Cam Config"**), open [config.html](config.html) in Chrome/Edge, connect, customize actions, and save to NVS.

> **Note**: If you re-flash via SWD (ST-Link), you must **forget/unpair** the device on your phone first. UF2 flashing preserves NVS storage if flash layout is unchanged, though re-pairing may still be needed if keys are cleared.

---

## 4. CI / GitHub Actions

The project includes a GitHub Actions workflow (`.github/workflows/build.yml`) that:
- Caches the **Zephyr SDK** and **west modules**.
- Runs the host-based **GoogleTest unit tests** (`./scripts/test.sh`).
- Builds the firmware automatically on push/PR for `promicro_nrf52840/nrf52840/uf2`.
- Uploads `zephyr.hex` and `zephyr.uf2` as downloadable GitHub Actions artifacts.

---

## 5. Bluetooth Details
- **Device Names**: `Cam Remote Pro` (HID Slots `0–2`), `Cam Config` (Config Slot `3`)
- **Appearance**: Keyboard (`961`) — ensures Windows maps Consumer Control buttons natively.
- **PnP ID**: Vendor `0x05AC`, Product `0x0220` (Apple Vendor ID used for driver-less Windows/iOS compatibility)
- **Security**: Just Works (`BT_SECURITY_L2`) by default; optional Passkey/MITM (`BT_SECURITY_L4`) via `CONFIG_SAMPLE_BT_USE_AUTHENTICATION=y`.
- **Advertising Watchdog**: Background worker toggles between directed (to bonded peer) and undirected advertising on bonded HID slots, and keeps undirected advertising active on unbonded slots or Config Mode.
- **Persistent Bonding & Mappings**: Active slot, per-slot peer addresses, and `ConfigMappings` are stored in NVS.
- **Multi-Device Profiles**: 4 slots total (`0–2` HID devices with separate BLE identities, `3` Web Bluetooth Config Mode); cycle via encoder button long-press.

---

## 6. Hardware & 3D Modeling (Coming Soon)

---

## 7. Unit Testing

The project uses **GoogleTest** for host-based unit testing of the core logic (`GestureEngine`, `ProfileManager`, and `battery_conversion`). These tests run natively on Windows (via `scripts/build.py --test`) or Linux/WSL (via `scripts/test.sh`) without requiring nRF52 hardware.

### Test Directory Structure
- `firmware/tests/`: Contains the test source code and `CMakeLists.txt`.
- `firmware/build_host/` (Linux/WSL) and `firmware/build_host_win/` (Windows): Host test build directories.
