---
description: Functional and non-functional requirements for the nRF52840 BLE Camera Remote.
---
# Functional & Non-Functional Requirements

This document tracks the current required behavior and constraints of the nRF52840 BLE Camera Remote. 

## 1. Functional Requirements (FR)

### FR-01: BLE HID Interface
The device must act as a Bluetooth Low Energy (BLE) Human Interface Device (HID) using the Consumer Control (Media Remote) profile.

### FR-02: Camera Trigger (Main Button)
- **Single Click**: Send a HID 'Volume Up' command (triggers phone shutter).
- **Long Press (>800ms)**: Send and hold a HID 'Volume Up' command (triggers Burst mode on modern smartphones).
- **Debounce**: All button presses must be debounced (standard: 60ms).

### FR-03: Rotary Encoder Support
- **Clockwise (CW)**: Send HID 'Volume Up'.
- **Counter-Clockwise (CCW)**: Send HID 'Volume Down'.
- **Encoder Button Short Click**: Send HID 'Mute' toggle.
- **Encoder Button Long Press (>800ms)**: Cycle active device profile (see FR-06).

### FR-04: Audio/Visual Feedback
- **Acoustic**: Short 4kHz PWM beep for clicks; long 50ms beep for entry into Long Press/Burst mode.
- **Profile Switch**: N+1 short beeps to indicate slot N (1 beep = slot 0, 2 = slot 1, 3 = slot 2).
- **Visual (RGB LED)**:
    - **Blue (Blinking)**: Advertising / Pairing mode.
    - **Green**: Connected.
    - **Red**: Trigger event (photo/burst) active.
- **Power Save**: LEDs should dim or disable during idle states to prolong battery life.

### FR-05: Battery Monitoring
- Monitor internal VDD via SAADC.
- Report battery percentage (0-100%) via BLE Battery Service (BAS).
- Update frequency: once per 60 seconds.

### FR-06: Multi-Device Profiles
- Support bonding with up to **3 devices** (`CONFIG_BT_MAX_PAIRED=3`).
- Only **one connection active** at a time.
- User cycles the active profile slot via encoder button long-press.
- On switch: disconnect current peer → advertising watchdog re-advertises.
- Active slot index persisted in NVS across power cycles.

### FR-07: Provisioning Service (Future)
- Custom BLE GATT service (vendor-specific 128-bit UUID) for button-function programming.
- Skeleton only: accepts writes to a button-map characteristic but takes no action.
- Will eventually allow a companion app to configure what each button/gesture does per profile.

## 2. Non-Functional Requirements (NFR)

### NFR-01: Power Efficiency
- Must utilize Zephyr's low-power idle (System ON).
- Interrupt-driven wake-up for all buttons and encoder events (no continuous polling of GPIOs).

### NFR-02: Compatibility
- Use Apple Vendor PnP ID (`0x05AC`) to ensure driver-less compatibility with Windows and iOS camera apps.

### NFR-03: Reliability
- **Advertising Watchdog**: Automatically restart BLE advertising if it stops unexpectedly or after a timeout without connection.
- **Persistent Bonding**: Store pairing information in NVS to allow automatic reconnection.

### NFR-04: Testability
- 100% logic coverage for the gesture engine and battery conversion logic on the host (Linux/WSL) using GoogleTest.

### NFR-05: Performance
- Input latency for button triggers must be <20ms from debounce completion.
