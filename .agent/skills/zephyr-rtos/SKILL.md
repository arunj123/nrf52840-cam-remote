# Zephyr RTOS Skill

This skill provides knowledge for working with Zephyr RTOS and hardware peripherals.

## 1. Power Management
- Utilize **System ON** idle mode for low power.
- Use interrupt-driven wake-ups for GPIO events.
- Enable DC/DC mode for lower quiescent current.

## 2. Hardware Interfacing
- **SAADC**: Used for internal VDD monitoring (battery level).
- **PWM**: Used for audio feedback (buzzer).
- **NVS**: Used for persistent storage of bonding info and active profile slot.

## 3. Modern C++ in Zephyr
- Ensure the build system is configured for C++23.
- Use Zephyr-specific C++ abstractions where applicable.
