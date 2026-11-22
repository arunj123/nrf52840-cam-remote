/** @file
 *  @brief HoG Service class (Modern C++20)
 */

#pragma once

#include <array>
#include <cstdint>

namespace remote {

static constexpr uint8_t kNumHidSlots = 3;

/// Action codes stored in ConfigMappings and dispatched across 3 HID reports:
/// - Report ID 1 (Consumer Control bitmask): 0x00, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80
/// - Report ID 2 (Mouse & Scrolling):        0x21..0x29
/// - Report ID 3 (Keyboard & Shortcuts):     0x30..0x3F, 0x41..0x54
enum class HidKey : uint8_t {
    None               = 0x00,

    // Consumer Control (Report ID 1)
    VolumeUp           = 0x01,  // Volume Increment (Camera Shutter)
    VolumeDown         = 0x02,  // Volume Decrement
    Mute               = 0x04,  // Mute
    PlayPause          = 0x08,  // Play/Pause (Video Start/Stop)
    NextTrack          = 0x10,  // Scan Next Track
    PrevTrack          = 0x20,  // Scan Previous Track
    BrowserBack        = 0x40,  // AC Back
    BrowserForward     = 0x80,  // AC Forward

    // Mouse & Scrolling (Report ID 2)
    ScrollUp           = 0x21,  // Vertical Scroll Up (+1)
    ScrollDown         = 0x22,  // Vertical Scroll Down (-1)
    ScrollUpFast       = 0x23,  // Vertical Scroll Up Fast (+3)
    ScrollDownFast     = 0x24,  // Vertical Scroll Down Fast (-3)
    ScrollLeft         = 0x25,  // Horizontal Scroll Left (-1 AC Pan)
    ScrollRight        = 0x26,  // Horizontal Scroll Right (+1 AC Pan)
    MouseLeftClick     = 0x27,  // Mouse Left Button
    MouseRightClick    = 0x28,  // Mouse Right Button
    MouseMiddleClick   = 0x29,  // Mouse Middle Button

    // Keyboard Navigation & Function Keys (Report ID 3)
    KeySpace           = 0x30,  // Spacebar
    KeyEnter           = 0x31,  // Enter
    KeyEscape          = 0x32,  // Escape
    KeyTab             = 0x33,  // Tab
    KeyBackspace       = 0x34,  // Backspace
    KeyUp              = 0x35,  // Up Arrow
    KeyDown            = 0x36,  // Down Arrow
    KeyLeft            = 0x37,  // Left Arrow
    KeyRight           = 0x38,  // Right Arrow
    KeyPageUp          = 0x39,  // Page Up
    KeyPageDown        = 0x3A,  // Page Down
    KeyHome            = 0x3B,  // Home
    KeyEnd             = 0x3C,  // End
    KeyF5              = 0x3D,  // F5 (Refresh / Start Slideshow)
    KeyFullscreen      = 0x3E,  // F Key (Video Fullscreen)
    KeyPrintScreen     = 0x3F,  // Print Screen

    // Keyboard Shortcuts (Report ID 3 with Modifiers)
    ShortcutCopy       = 0x41,  // Ctrl + C
    ShortcutPaste      = 0x42,  // Ctrl + V
    ShortcutCut        = 0x43,  // Ctrl + X
    ShortcutUndo       = 0x44,  // Ctrl + Z
    ShortcutRedo       = 0x45,  // Ctrl + Y
    ShortcutSelectAll  = 0x46,  // Ctrl + A
    ShortcutSave       = 0x47,  // Ctrl + S
    ShortcutFind       = 0x48,  // Ctrl + F
    ShortcutZoomIn     = 0x49,  // Ctrl + +
    ShortcutZoomOut    = 0x4A,  // Ctrl + -
    ShortcutZoomReset  = 0x4B,  // Ctrl + 0
    ShortcutNewTab     = 0x4C,  // Ctrl + T
    ShortcutCloseTab   = 0x4D,  // Ctrl + W
    ShortcutReopenTab  = 0x4E,  // Ctrl + Shift + T
    ShortcutNextTab    = 0x4F,  // Ctrl + Tab
    ShortcutPrevTab    = 0x50,  // Ctrl + Shift + Tab
    ShortcutAltTab     = 0x51,  // Alt + Tab
    ShortcutShowDesktop = 0x52, // Win + D
    ShortcutLockScreen = 0x53,  // Win + L
    ShortcutSnippingTool = 0x54,// Win + Shift + S
};

[[nodiscard]] constexpr bool is_valid_hid_key(uint8_t raw) noexcept {
    switch (static_cast<HidKey>(raw)) {
        case HidKey::None:
        case HidKey::VolumeUp:
        case HidKey::VolumeDown:
        case HidKey::Mute:
        case HidKey::PlayPause:
        case HidKey::NextTrack:
        case HidKey::PrevTrack:
        case HidKey::BrowserBack:
        case HidKey::BrowserForward:
        case HidKey::ScrollUp:
        case HidKey::ScrollDown:
        case HidKey::ScrollUpFast:
        case HidKey::ScrollDownFast:
        case HidKey::ScrollLeft:
        case HidKey::ScrollRight:
        case HidKey::MouseLeftClick:
        case HidKey::MouseRightClick:
        case HidKey::MouseMiddleClick:
        case HidKey::KeySpace:
        case HidKey::KeyEnter:
        case HidKey::KeyEscape:
        case HidKey::KeyTab:
        case HidKey::KeyBackspace:
        case HidKey::KeyUp:
        case HidKey::KeyDown:
        case HidKey::KeyLeft:
        case HidKey::KeyRight:
        case HidKey::KeyPageUp:
        case HidKey::KeyPageDown:
        case HidKey::KeyHome:
        case HidKey::KeyEnd:
        case HidKey::KeyF5:
        case HidKey::KeyFullscreen:
        case HidKey::KeyPrintScreen:
        case HidKey::ShortcutCopy:
        case HidKey::ShortcutPaste:
        case HidKey::ShortcutCut:
        case HidKey::ShortcutUndo:
        case HidKey::ShortcutRedo:
        case HidKey::ShortcutSelectAll:
        case HidKey::ShortcutSave:
        case HidKey::ShortcutFind:
        case HidKey::ShortcutZoomIn:
        case HidKey::ShortcutZoomOut:
        case HidKey::ShortcutZoomReset:
        case HidKey::ShortcutNewTab:
        case HidKey::ShortcutCloseTab:
        case HidKey::ShortcutReopenTab:
        case HidKey::ShortcutNextTab:
        case HidKey::ShortcutPrevTab:
        case HidKey::ShortcutAltTab:
        case HidKey::ShortcutShowDesktop:
        case HidKey::ShortcutLockScreen:
        case HidKey::ShortcutSnippingTool:
            return true;
    }
    return false;
}

/// User-configurable action mappings for a single HID profile slot
struct ConfigMappings {
    HidKey main_button_single;
    HidKey main_button_long;
    HidKey encoder_cw;
    HidKey encoder_ccw;
    HidKey encoder_button_short;
    HidKey secondary_button_single;
    HidKey secondary_button_long;
};
static_assert(sizeof(ConfigMappings) == 7, "ConfigMappings must be 7 bytes for BLE/NVS layout");

static constexpr ConfigMappings kDefaultConfigMappings = {
    .main_button_single = HidKey::VolumeUp,
    .main_button_long = HidKey::VolumeUp,
    .encoder_cw = HidKey::VolumeUp,
    .encoder_ccw = HidKey::VolumeDown,
    .encoder_button_short = HidKey::Mute,
    .secondary_button_single = HidKey::PlayPause,
    .secondary_button_long = HidKey::NextTrack,
};

using AllSlotMappings = std::array<ConfigMappings, kNumHidSlots>;
static_assert(sizeof(AllSlotMappings) == 21, "AllSlotMappings must be 21 bytes");

static constexpr uint8_t kLegacyMappingSize = 5;
static constexpr uint8_t kLegacyAllSlotMappingsSize = kLegacyMappingSize * kNumHidSlots;

/// Unpack a legacy 5-byte ConfigMappings payload, defaulting secondary button actions
[[nodiscard]] constexpr ConfigMappings unpack_legacy_mapping(const uint8_t* bytes) noexcept {
    return {
        .main_button_single = static_cast<HidKey>(bytes[0]),
        .main_button_long = static_cast<HidKey>(bytes[1]),
        .encoder_cw = static_cast<HidKey>(bytes[2]),
        .encoder_ccw = static_cast<HidKey>(bytes[3]),
        .encoder_button_short = static_cast<HidKey>(bytes[4]),
        .secondary_button_single = kDefaultConfigMappings.secondary_button_single,
        .secondary_button_long = kDefaultConfigMappings.secondary_button_long,
    };
}

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

    /// Set mappings for a specific HID slot (0..2)
    static void set_slot_mappings(uint8_t slot, const ConfigMappings& mappings);

    /// Get mappings for a specific HID slot (0..2; out-of-range defaults to slot 0)
    static ConfigMappings get_slot_mappings(uint8_t slot);

    /// Set mappings for all 3 HID slots
    static void set_all_slot_mappings(const AllSlotMappings& mappings);

    /// Get mappings for all 3 HID slots
    static AllSlotMappings get_all_slot_mappings();

    /// Set configurable mappings across all HID slots (legacy helper)
    static void set_config_mappings(const ConfigMappings& mappings);
    
    /// Get configurable mappings for the active slot
    static ConfigMappings get_config_mappings();
};

} // namespace remote
