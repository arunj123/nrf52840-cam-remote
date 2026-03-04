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
- **Encoder Button Click**: Send HID 'Mute' toggle.

### FR-04: Audio/Visual Feedback
- **Acoustic**: Short 4kHz PWM beep for clicks; long 50ms beep for entry into Long Press/Burst mode.
- **Visual (RGB LED)**:
    - **Blue (Blinking)**: Advertising / Pairing mode.
    - **Green**: Connected.
    - **Red**: Trigger event (photo/burst) active.
- **Power Save**: LEDs should dim or disable during idle states to prolong battery life.

### FR-05: Battery Monitoring
- Monitor internal VDD via SAADC.
- Report battery percentage (0-100%) via BLE Battery Service (BAS).
- Update frequency: once per 60 seconds.

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
