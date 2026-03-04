# nRF52840 BLE Camera Remote

A Bluetooth Low Energy HID Camera Remote using the MakerDiary nRF52840 MDK USB Dongle and Zephyr RTOS. Triggers a smartphone camera shutter (via Volume Up HID command) using a physical button or by shorting GPIO pins.

## 1. Hardware Setup

### The Device
- **Board**: [MakerDiary nRF52840 MDK USB Dongle](https://wiki.makerdiary.com/nrf52840-mdk-usb-dongle/)
- **Core**: Nordic nRF52840 (Cortex-M4F, Bluetooth 5.0)

### Wiring for Flashing (ST-Link V2)
To recover or flash the device manually (if the USB bootloader is missing or locked), connect an **ST-Link V2** to the 4-pin header:
- **SWDIO**: Signal Data
- **SWCLK**: Signal Clock
- **GND**: Common Ground
- **3.3V**: Power (The board must be powered to flash)

### External Trigger Button & Buzzer
- **Trigger Pin (Primary)**: Pin labeled **P4** (P0.04) shorted to **GND**.
- **Trigger Pin (Secondary)**: Pin labeled **P5** (P0.05) shorted to **GND**.
- **Buzzer Feedback**: Connect a **Passive Ceramic Piezo** wafer between **P2** (P0.02) and **GND**.
- **Action**: Triggers a Red LED pulse, a **50ms (4kHz) acoustic beep**, and sends the Bluetooth "Volume Up" command.

### Rotary Encoder (Optional)
For volume control, a rotary encoder (e.g., EC11) can be connected to the following pins on Header 1:

| Dongle Header Pin | nRF52840 GPIO | Encoder Component | Function |
| :--- | :--- | :--- | :--- |
| **Pin 5** | `P0.03` | **Phase A** | Quad Decoder A |
| **Pin 8** | `P0.06` | **Phase B** | Quad Decoder B |
| **Pin 9** | `P0.07` | **Switch (Button)** | Mute Toggle |
| **Pin 2** | **GND** | **Common (C)** | Ground Reference |

> **Note**: Internal pull-up resistors are enabled in software, so no external pull-ups are required for mechanical encoders.

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
The project is built using **Modern C++20** for improved modularity and safety.
- **OS**: Zephyr RTOS v4.x
- **Standard**: C++20 (using `std::span`, `std::array`, and structured classes)
- **Logic**: Encapsulated in `remote::HidService`, `remote::BluetoothManager`, `remote::LedController`, `remote::BuzzerController`, and `remote::BatteryMonitor`.
- **Button Engine**: Interrupt-driven thread with automated power-saving sleep. Deadlock-proof.
- **Profile**: Consumer Control (Media Remote) via HOGP.
- **Pairing**: "Just Works" (no PIN required, encrypted link with bonding).

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

# 4. Build the firmware
./scripts/build.sh

# 5. Build + copy hex to Windows for flashing
./scripts/build.sh --flash
```

### Build Scripts

| Script | Purpose |
|--------|---------|
| `scripts/setup_env.sh` | Installs system deps, creates Python venv, initializes Zephyr workspace, downloads SDK 0.17.0 |
| `scripts/setup_env.sh --ci` | Same as above but quieter output for CI |
| `scripts/build.sh` | Activates venv and runs `west build` for `nrf52840_mdk` |
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
west build -b nrf52840_mdk -d <project>/firmware/build <project>/firmware
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

### Step 3: Pair with Phone
1. On Android: **Settings → Bluetooth → Scan**.
2. Pair with **"Cam Remote Pro"** (no PIN required).
3. Open the Camera app and short **P4** or **P5** to GND to take a photo.

> **Note**: If you re-flash, you must **forget/unpair** the device on your phone first, then re-pair. The mass erase clears the bonding info on the dongle.

---

## 4. CI / GitHub Actions

The project includes a GitHub Actions workflow (`.github/workflows/build.yml`) that:
- Caches the **Zephyr SDK** (~1.5 GB) and **west modules** between runs.
- Builds the firmware automatically on push/PR to the **`master`** branch.
- Uploads `zephyr.hex` as a downloadable GitHub Actions artifact.

---

## 5. Bluetooth Details
- **Device Name**: `Cam Remote Pro`
- **Appearance**: Remote Control (384)
- **PnP ID**: Vendor `0x05AC`, Product `0x0220` (Apple Vendor ID used for driver-less Windows compatibility)
- **Security**: "Just Works" encryption (BT_SECURITY_L2). No passkey required.
- **Advertising Watchdog**: Background worker ensures advertising automatically restarts if the BLE stack hits an error or if the device disconnects.
- **Persistent Bonding**: Pairing info is stored in NVS, allowing auto-reconnection after power cycles.

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
