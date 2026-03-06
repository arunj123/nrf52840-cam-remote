# Project Requirements

This document tracks the required behavior and constraints of the nRF52840 BLE Camera Remote.

## 1. Functional Requirements (FR)

### BLE & HID
- **FR-01**: Act as a BLE HID using the Consumer Control profile.
- **FR-02**: Main Button Click -> 'Volume Up' (Shutter); Long Press -> Burst Mode.
- **FR-03**: Encoder C/W -> 'Volume Up', CC/W -> 'Volume Down'; Button Click -> 'Mute'; Long Press -> Cycle Profile.

### Feedback & Interface
- **FR-04**: 4kHz PWM beep for clicks; 50ms beep for Long Press.
- **FR-05**: RGB LED: Blue (Advertising), Green (Connected), Red (Trigger). Profile index feedback via beeps.

### System & Persistence
- **FR-06**: Support up to 3 bonded devices. Persistence in NVS.
- **FR-07**: Provisioning Service (GATT) for future config.

## 2. Non-Functional Requirements (NFR)
- **NFR-01**: Power Efficiency (Zephyr System ON, Interrupt-driven).
- **NFR-02**: Compatibility (Apple Vendor PnP ID `0x05AC`).
- **NFR-03**: Reliability (Advertising Watchdog, Persistent Bonding).
- **NFR-04**: Testability (100% logic coverage on host).
- **NFR-05**: Performance (Latency <20ms).
