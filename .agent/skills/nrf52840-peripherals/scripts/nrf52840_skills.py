"""
nRF52840 Zephyr OS Agent Skills
Version: 1.0
Target: Nordic Semiconductor nRF52840 (Product Spec v1.11)
OS: Zephyr RTOS

Description:
This module provides skills (tools) for an AI agent to generate valid Zephyr OS
configurations (Devicetree, Kconfig, and C code) specifically tailored to the 
nRF52840 SoC.

References:
- nRF52840 Product Specification: https://infocenter.nordicsemi.com/topic/ps_nrf52840/keyfeatures_html5.html
- Zephyr Devicetree API: https://docs.zephyrproject.org/latest/build/dts/api/api.html
- Zephyr Bluetooth: https://docs.zephyrproject.org/latest/connectivity/bluetooth/index.html
"""

import textwrap
from typing import Dict

def generate_gpio_overlay(port: int, pin: int, label: str, direction: str = "output") -> str:
    """
    Generates a Zephyr Devicetree overlay snippet for an nRF52840 GPIO pin.
    
    Hardware Constraints (per nRF52840 PS v1.11 Section 6.8):
    - Port 0 (P0): Pins 0 to 31
    - Port 1 (P1): Pins 0 to 15
    
    Args:
        port: The GPIO port number (0 or 1).
        pin: The pin number (0-31 for port 0, 0-15 for port 1).
        label: The Devicetree node label (e.g., 'led_1', 'button_1').
        direction: 'output' for LEDs/actuators, 'input' for buttons/sensors.
        
    Returns:
        A string containing the formatted .overlay Devicetree snippet.
    """
    if port == 0 and not (0 <= pin <= 31):
        return "Error: nRF52840 Port 0 only supports pins 0-31."
    if port == 1 and not (0 <= pin <= 15):
        return "Error: nRF52840 Port 1 only supports pins 0-15."
        
    flags = "GPIO_ACTIVE_HIGH"
    if direction == "input":
        flags += " | (GPIO_PULL_UP | GPIO_ACTIVE_LOW)"
        
    overlay = textwrap.dedent(f"""\
    / {{
        aliases {{
            {label.replace('_', '')} = &{label};
        }};

        {label}s {{
            compatible = "gpio-keys"; /* or gpio-leds */
            {label}: {label}_{port}_{pin} {{
                gpios = <&gpio{port} {pin} {flags}>;
                label = "{label.replace('_', ' ').title()}";
            }};
        }};
    }};
    """)
    return overlay

def generate_ble_kconfig(role: str = "peripheral") -> str:
    """
    Generates Zephyr Kconfig parameters to enable the nRF52840 Bluetooth 5 Radio.
    
    Hardware Capabilities (per nRF52840 PS v1.11):
    - 2.4 GHz transceiver
    - Bluetooth 5, 2 Mbps, 1 Mbps, 500 kbps, and 125 kbps (Long Range)
    
    Args:
        role: "peripheral" or "central".
        
    Returns:
        A string containing the prj.conf configuration lines.
    """
    base_config = textwrap.dedent("""\
    # nRF52840 BLE Configuration
    CONFIG_BT=y
    CONFIG_BT_DEVICE_NAME="nRF52840_Device"
    # Enable the Nordic SoftDevice/Zephyr Link Layer
    CONFIG_BT_LL_SW_SPLIT=y
    """)
    
    if role == "peripheral":
        base_config += "CONFIG_BT_PERIPHERAL=y\n"
        base_config += "CONFIG_BT_DIS=y\n" # Device Information Service
    elif role == "central":
        base_config += "CONFIG_BT_CENTRAL=y\n"
        base_config += "CONFIG_BT_GATT_CLIENT=y\n"
        
    return base_config

def generate_saadc_overlay(ain_channel: int, resolution: int = 12) -> str:
    """
    Generates a Devicetree overlay for the nRF52840 Successive Approximation ADC (SAADC).
    
    Hardware Constraints (per nRF52840 PS v1.11 Section 6.22):
    - nRF52840 has 8 analog inputs (AIN0 to AIN7).
    - Resolutions supported: 8, 10, 12, or 14-bit.
    
    Args:
        ain_channel: Analog input channel (0-7).
        resolution: ADC resolution in bits (8, 10, 12, 14).
        
    Returns:
        A string containing the ADC Devicetree overlay and Kconfig snippet.
    """
    if not (0 <= ain_channel <= 7):
        return "Error: nRF52840 only supports AIN channels 0 through 7."
    if resolution not in [8, 10, 12, 14]:
        return "Error: SAADC resolution must be 8, 10, 12, or 14 bits."
        
    overlay = textwrap.dedent(f"""\
    /* Devicetree Overlay */
    &adc {{
        status = "okay";
        #address-cells = <1>;
        #size-cells = <0>;

        channel@{ain_channel} {{
            reg = <{ain_channel}>;
            zephyr,gain = "ADC_GAIN_1_6";
            zephyr,reference = "ADC_REF_INTERNAL";
            zephyr,acquisition-time = <ADC_ACQ_TIME_DEFAULT>;
            zephyr,input-positive = <NRF_SAADC_AIN{ain_channel}>; /* P0.0{2+ain_channel} */
            zephyr,resolution = <{resolution}>;
        }};
    }};
    
    /* Required prj.conf */
    # CONFIG_ADC=y
    # CONFIG_ADC_NRFX_SAADC=y
    """)
    return overlay

def generate_i2c_overlay(instance: int, scl_port: int, scl_pin: int, sda_port: int, sda_pin: int) -> str:
    """
    Generates a Devicetree overlay for an nRF52840 I2C (TWIM) interface using pinctrl.
    
    Hardware Constraints:
    - nRF52840 has two TWIM (I2C) master instances: 0 and 1.
    - Pins can be mapped to any valid GPIO.
    
    Args:
        instance: TWIM instance (0 or 1).
        scl_port, scl_pin: Port and pin for SCL.
        sda_port, sda_pin: Port and pin for SDA.
        
    Returns:
        Devicetree overlay string utilizing Zephyr's pinctrl paradigm.
    """
    if instance not in [0, 1]:
        return "Error: nRF52840 TWIM instances are 0 or 1."
        
    overlay = textwrap.dedent(f"""\
    &pinctrl {{
        i2c{instance}_default: i2c{instance}_default {{
            group1 {{
                psels = <NRF_PSEL(TWIM_SCL, {scl_port}, {scl_pin})>,
                        <NRF_PSEL(TWIM_SDA, {sda_port}, {sda_pin})>;
            }};
        }};
        i2c{instance}_sleep: i2c{instance}_sleep {{
            group1 {{
                psels = <NRF_PSEL(TWIM_SCL, {scl_port}, {scl_pin})>,
                        <NRF_PSEL(TWIM_SDA, {sda_port}, {sda_pin})>;
                low-power-enable;
            }};
        }};
    }};

    &i2c{instance} {{
        compatible = "nordic,nrf-twim";
        status = "okay";
        pinctrl-0 = <&i2c{instance}_default>;
        pinctrl-1 = <&i2c{instance}_sleep>;
        pinctrl-names = "default", "sleep";
        clock-frequency = <I2C_BITRATE_FAST>;
    }};
    """)
    return overlay

if __name__ == "__main__":
    # Test the agent skills
    print("--- nRF52840 AI Agent Zephyr Skills Loaded ---")
    print(generate_gpio_overlay(port=1, pin=10, label="status_led", direction="output"))
    print(generate_saadc_overlay(ain_channel=2, resolution=14))
