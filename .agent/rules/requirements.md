# Project Requirements

This document tracks the required behavior and constraints of the nRF52840 BLE Camera Remote.

## 1. Functional Requirements (FR)

### BLE & HID
- **FR-01**: Act as a composite 3-report BLE HID (Consumer Control, Mouse Wheel/Clicks, and Keyboard/Shortcuts) on HID slots `0–2` (`Cam Remote Pro 1`, `Cam Remote Pro 2`, `Cam Remote Pro 3`), with a dedicated Web Bluetooth configuration profile on slot `3` (`Cam Config`).
- **FR-02**: Main Button (`D5`) Click (`< 500ms`) -> configurable `main_button_single` (default `Volume Up`); Long Hold (`500ms–5s`) -> Burst Mode (`main_button_long`, default `Volume Up` held 2s); Hold (`>= 5s`) -> clear active slot bond and rotate BT identity.
- **FR-03**: Second Button (`D4`) Click (`< 500ms`) -> configurable `secondary_button_single` (default `Play / Pause`); Long Press (`>= 500ms`) -> configurable `secondary_button_long` (default `Next Track`).
- **FR-04**: Encoder C/W -> `encoder_cw` (default `Volume Up`), CC/W -> `encoder_ccw` (default `Volume Down`); Encoder Button Short Click -> `encoder_button_short` (default `Mute`); Long Press (`>= 500ms`, including initial debounce) -> Cycle Slot (`0 → 1 → 2 → 3 → 0`).

### Feedback & Interface
- **FR-05**: 4kHz PWM buzzer (`D6` / `P0.22`): 50ms short beep on single click, 300ms long beep on burst/secondary long press, `N+1` beeps when switching to slot `N`, and 3 countdown beeps + long beep on bond clear.
- **FR-06**: Onboard Red LED (`P0.15`) illuminates while a trigger gesture is active and blinks `N+1` times in sync with the buzzer on profile switch.

### System & Persistence
- **FR-07**: Support 4 total profile slots (Slots `0–2` bonded HID devices + Slot `3` Config Mode). Active slot, peer addresses, and per-slot button/encoder mappings (`AllSlotMappings`, 21 bytes across Slots `0–2`) persist in NVS (`settings` subsystem).
- **FR-08**: Custom Config Service (`9e4571a5-b7d9-4daa-a3e5-a8f086007a2b`) exposed on Slot `3` for reading/writing `AllSlotMappings` (with 15/7/5-byte backward compatibility) via `config.html` (Web Bluetooth).

## 2. Non-Functional Requirements (NFR)
- **NFR-01**: Power Efficiency (Zephyr System ON idle, GPIO edge-to-active interrupts, thread-deferred QDEC sensor trigger, LDO regulator mode for clone compatibility).
- **NFR-02**: Compatibility (Apple Vendor PnP ID `0x05AC`, Product `0x0220`, Keyboard appearance `961`).
- **NFR-03**: Reliability (Advertising Watchdog, Persistent Bonding, zeroed bond cleanup via `settings_delete`, thread-safe GATT/connection state).
- **NFR-04**: Testability (Host-based GoogleTest suite runnable on Windows via `scripts/build.py --test` and Linux/WSL via `scripts/test.sh`).
- **NFR-05**: Performance (Latency <20ms; zero blocking sleeps inside ISRs or BT ready callbacks).
