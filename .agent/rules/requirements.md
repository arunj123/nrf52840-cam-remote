# Project Requirements

This document tracks the required behavior and constraints of the nRF52840 BLE Camera Remote.

## 1. Functional Requirements (FR)

### BLE & HID
- **FR-01**: Act as a BLE HID using the Consumer Control profile on HID slots `0–2` (`Cam Remote Pro`), with a dedicated Web Bluetooth configuration profile on slot `3` (`Cam Config`).
- **FR-02**: Main Button Click (`< 800ms`) -> configurable `main_button_single` (default `Volume Up`); Long Hold (`800ms–5s`) -> Burst Mode (`main_button_long`, default `Volume Up` held 2s); Hold (`>= 5s`) -> clear active slot bond and rotate BT identity.
- **FR-03**: Encoder C/W -> `encoder_cw` (default `Volume Up`), CC/W -> `encoder_ccw` (default `Volume Down`); Encoder Button Short Click -> `encoder_button_short` (default `Mute`); Long Press (`>= 800ms`) -> Cycle Slot (`0 → 1 → 2 → 3 → 0`).

### Feedback & Interface
- **FR-04**: 4kHz PWM buzzer (`D6` / `P0.22`): 50ms short beep on single click, 300ms long beep on burst mode, `N+1` beeps when switching to slot `N`, and 3 countdown beeps + long beep on bond clear.
- **FR-05**: Onboard Red LED (`P0.15`) illuminates while a trigger gesture is active.

### System & Persistence
- **FR-06**: Support 4 total profile slots (Slots `0–2` bonded HID devices + Slot `3` Config Mode). Active slot, peer addresses, and custom button/encoder mappings persist in NVS (`settings` subsystem).
- **FR-07**: Custom Config Service (`9e4571a5-b7d9-4daa-a3e5-a8f086007a2b`) exposed on Slot `3` for reading/writing `ConfigMappings` via `config.html` (Web Bluetooth).

## 2. Non-Functional Requirements (NFR)
- **NFR-01**: Power Efficiency (Zephyr System ON idle, GPIO edge-to-active interrupts, thread-deferred QDEC sensor trigger, LDO regulator mode for clone compatibility).
- **NFR-02**: Compatibility (Apple Vendor PnP ID `0x05AC`, Product `0x0220`, Keyboard appearance `961`).
- **NFR-03**: Reliability (Advertising Watchdog, Persistent Bonding, zeroed bond cleanup via `settings_delete`, thread-safe GATT/connection state).
- **NFR-04**: Testability (Host-based GoogleTest suite runnable on Windows via `scripts/build.py --test` and Linux/WSL via `scripts/test.sh`).
- **NFR-05**: Performance (Latency <20ms; zero blocking sleeps inside ISRs or BT ready callbacks).
