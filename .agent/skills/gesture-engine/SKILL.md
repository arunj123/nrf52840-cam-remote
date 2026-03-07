# Zephyr Gesture Engine Skill

## 1. Description
Provides knowledge for implementing and tuning the gesture detection logic for buttons and rotary encoders in the `nrf_control` project.

## 2. Triggers
- "Modify button debounce timing"
- "Add new gesture state"
- "Update long press threshold"
- "Implement burst mode logic"

## 3. Gesture Logic (from gesture_logic.md)
The system uses a state-machine approach for handling input events.

### Main Button States
- **Idle**: Waiting for wake event.
- **Debouncing**: 60ms noise filtering.
- **Single Click**: Short press (<800ms).
- **Long Press Detection**: Continuous polling at 20ms intervals.
- **Burst Mode**: Triggered after 800ms hold. High-speed volume/action updates.
- **Wait for Release**: Ensuring button is up before returning to Idle.

### Encoder Button States
- **Mute Toggle**: Short press (<800ms) on the encoder knob.
- **Profile Switch**: Long press (>800ms) to cycle through device profiles.

## 4. Hardware/Software Integration
- **GPIO SENSE**: Used for low-power wake-up from System OFF.
- **Kconfig**: Ensure `CONFIG_GPIO=y` and `CONFIG_BT_HID_REPORT_MAP` are correctly set for the target profile.
- **Timing Parameters**:
  - `kDebounceMs`: 60ms
  - `kLongPressMs`: 800ms
  - `kBurstHoldMs`: 2000ms
  - `kPollMs`: 20ms

## 5. Implementation Patterns
- Use `k_work_delayable` for non-blocking timing.
- Implement state transitions within the GPIO callback or a dedicated manager thread.
- Ensure thread safety for shared state (e.g., active profile index).
