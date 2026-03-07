# nRF52840 Peripherals Skill

This skill provides deep knowledge for configuring and using nRF52840 hardware peripherals, specifically tailored for the Zephyr RTOS environment.

## 1. Quadrature Decoder (QDEC)
- **Standard Usage**: Primary peripheral for rotary encoders.
- **Key Features**:
  - Accumulates counts automatically without CPU intervention.
  - Supports "Report" and "Sample" events.
  - Hard-wired debounce filters for noisy mechanical encoders.
- **Zephyr Integration**: Use `CONFIG_QDEC_NRFX=y` and the `sensor` API.

## 2. SAADC (Successive Approximation ADC)
- **Internal Monitoring**: Use the `VDD` or `VDDHDIV5` input for battery monitoring without external resistors.
- **Differential/Single-ended**: Highly configurable inputs.
- **Limits/Events**: Set hardware limits to trigger interrupts when battery voltage drops below a threshold.
- **Oversampling**: Use hardware oversampling (up to 256x) for higher precision and noise reduction.

## 3. GPIO & GPIOTE
- **Low Power**: Use "SENSE" mechanism in System OFF for wake-up.
- **High Drive**: P0.00 to P0.31 can support high drive (up to 5mA) if `H0H1` or `D0H1` is configured.
- **GPIOTE**: Limit the number of IN_EVENT channels to save power; use PORT events for miscellaneous buttons.

## 4. USBD (USB Device)
- **Concurrent Use**: Can be used alongside BLE for dual-mode HID.
- **Clock Requirements**: Requires a 48MHz clock derived from the HS crystal (HFCLK).
- **VBUS Detection**: Trigger sequences when USB is plugged in/out.

## 5. Automation & Code Generation
- **Scripted Generation**: Use the provided Python utility to generate Devicetree and Kconfig snippets.
- **Path**: `scripts/nrf52840_skills.py`
- **Features**: 
  - Validates nRF52840 pin ranges (P0.0-31, P1.0-15).
  - Generates pinctrl-aware I2C/TWIM overlays.
  - Configures SAADC channels with correct `AINx` mapping.
