# nRF52840 Power Management Skill

This skill provides expertise in optimizing power consumption for nRF52840-based devices, focusing on Zephyr RTOS implementations.

## 1. System States
- **System ON**: Default idle mode. CPU is sleeping, but peripherals (like BLE, Radio, DMA) can continue to operate.
- **System OFF**: Lowest power mode. Only wake-up via Reset, GPIO SENSE, or NFC. Flash contents are retained, but RAM retention must be explicitly enabled per block.

## 2. Power Regulators
- **LDO**: Standard linear regulator (less efficient).
- **DC/DC (REG1)**: Highly efficient switching regulator.
  - **Best Practice**: Always enable DC/DC if the inductor/capacitor circuit is present on the board. In Zephyr, use `CONFIG_BOARD_ENABLE_DCDC=y` (if supported) or call `nrfx_power_dcdc_mode_set(NRFX_POWER_DCDC_ENABLE)`.
- **REG0**: Used for High Voltage mode (VDDH) if the supply is 5V (USB).

## 3. RAM Retention
- In **System OFF**, RAM is usually powered down to save current. Use the `POWER` peripheral's `RAM[n].RETENTION` registers to preserve critical state (e.g., wake-up counters) if not using NVS.

## 4. Power Profiling
- **Current consumption**:
  - System OFF (no RAM retention): ~0.4 µA.
  - System ON (Base current): ~1.5 µA.
  - RADIO Tx (0 dBm): ~4.8 mA.
  - RADIO Rx (1 Mbps): ~4.6 mA.
