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

### External Trigger Button
- **Trigger Pin (Primary)**: Pin labeled **P4** (P0.04) shorted to **GND**.
- **Trigger Pin (Secondary)**: Pin labeled **P5** (P0.05) shorted to **GND**.
- **Internal Action**: Triggers a Red LED pulse and sends the Bluetooth "Volume Up" command.

### LED Feedback System
- **🔵 Blue**: Advertising (Looking for a pair).
- **🟢 Green**: Connected (Ready to use).
- **🔴 Red Pulse**: Trigger detected (Sending shutter command).
  - *Note: Green LED turns off during the Red pulse for better visibility.*

---

## 2. Software & Development

### Architecture
- **OS**: Zephyr RTOS v4.x
- **Stack**: BLE HID (HOGP)
- **Profile**: Consumer Control (Media Remote)
- **Pairing**: "Just Works" (no PIN required, encrypted link with bonding)

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
- Builds the firmware automatically on push/PR to `main`.
- Uploads `zephyr.hex` as a downloadable GitHub Actions artifact.

---

## 5. Bluetooth Details
- **Device Name**: `Cam Remote Pro`
- **Appearance**: Remote Control (384)
- **PnP ID**: Vendor `0x05AC`, Product `0x0220` (for driver-less Windows compatibility)
- **Security**: "Just Works" encryption (BT_SECURITY_L2). No passkey required.
- **Re-advertising**: The device automatically restarts advertising after disconnection.

---

## 6. Hardware & 3D Modeling (Coming Soon)

The `/hardware` directory is reserved for CAD files. 

### Integration Strategy:
1. **Case Design**: A two-part snap-fit or screw-together case (using M2 screws).
2. **Button Access**: Extenders for pins **P4** and **P5** to allow external triggering while the dongle is protected.
3. **Mounting**: A standard 1/4" tripod nut mount could be integrated for mounting on camera rigs.
4. **Material**: Recommended PETG or ABS for durability, though PLA is fine for prototyping.
