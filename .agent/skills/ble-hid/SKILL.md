# BLE HID Skill

This skill provides knowledge for managing Bluetooth Low Energy (BLE) Human Interface Device (HID) functionality.

## 1. HID Profiles
- **Consumer Control**: Used for media remote functionality (Volume Up/Down, Mute).
- **Appearance**: Remote Control (384).

## 2. Device Identification
- **PnP ID**: Vendor `0x05AC` (Apple), Product `0x0220`. This ensures driver-less compatibility with Windows and iOS.

## 3. Bonding & Multi-Profile
- Support for up to 3 bonded devices (`CONFIG_BT_MAX_PAIRED=3`).
- Use `nvs` to store the active profile slot index.
- Only one active connection is allowed at a time.
