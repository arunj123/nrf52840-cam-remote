/** @file
 *  @brief HoG Service class (Modern C++20)
 */

#pragma once

#include <cstdint>

namespace remote {

/// Consumer Control key bit positions in the HID report
enum class HidKey : uint8_t {
    None       = 0x00,
    VolumeUp   = 0x01,  // Bit 0: Volume Increment (Camera Shutter)
    VolumeDown = 0x02,  // Bit 1: Volume Decrement
    Mute       = 0x04,  // Bit 2: Mute
    PlayPause  = 0x08,  // Bit 3: Play/Pause (Video Start/Stop)
    NextTrack  = 0x10,  // Bit 4: Scan Next Track
    PrevTrack  = 0x20,  // Bit 5: Scan Previous Track
};

/// User-configurable action mappings for the custom profile
struct ConfigMappings {
    HidKey main_button_single;
    HidKey main_button_long;
    HidKey encoder_cw;
    HidKey encoder_ccw;
    HidKey encoder_button_short;
};
static_assert(sizeof(ConfigMappings) == 5, "ConfigMappings must be 5 bytes for BLE/NVS layout");

class HidService {
public:
    static void init();
    static void run_button_loop();

    /// Send a single key press + release
    static void send_key(HidKey key);

    /// Send a key press (hold down)
    static void send_key_down(HidKey key);

    /// Send a key release
    static void send_key_up();

    /// Set current configurable mappings
    static void set_config_mappings(const ConfigMappings& mappings);
    
    /// Get current configurable mappings
    static ConfigMappings get_config_mappings();
};

} // namespace remote
