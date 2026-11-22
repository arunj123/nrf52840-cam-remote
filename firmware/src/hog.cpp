/** @file
 *  @brief HoG Service Implementation (Modern C++20)
 *
 *  Implements a click timing engine with four gesture modes:
 *    - Single Click:  Photo (Volume Up)
 *    - Double Click:  Video Start/Stop (Play/Pause)
 *    - Long Press:    Burst Mode (hold Volume Up)
 *    - Triple Click:  Self-Timer (3-second countdown then photo)
 */

#include "hog.hpp"
#include "gesture_engine.hpp"

#include <zephyr/types.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/settings/settings.h>

#include <array>
#include <span>
#include <cstdint>

extern "C" void led_set_trigger_active(bool active);
extern "C" void buzzer_beep();
extern "C" void buzzer_long_beep();
extern "C" void on_profile_switch_request();
extern "C" void clear_bonds();

namespace remote {

namespace {
extern "C" uint8_t get_active_profile_id();

// ─── HID Service Internals ───────────────────────────────────────

enum class HidFlags : uint8_t {
    RemoteWake = BIT(0),
    NormallyConnectable = BIT(1),
};

struct hids_info {
    uint16_t version;
    uint8_t code;
    uint8_t flags;
} __packed;

struct hids_report {
    uint8_t id;
    uint8_t type;
} __packed;

constexpr hids_info info = {
    .version = 0x0111,
    .code = 0x00,
    .flags = static_cast<uint8_t>(HidFlags::NormallyConnectable),
};

enum class HidReportType : uint8_t {
    Input = 0x01,
    Output = 0x02,
    Feature = 0x03,
};

constexpr hids_report input_report_ref_1 = {
    .id = 0x01,
    .type = static_cast<uint8_t>(HidReportType::Input),
};

constexpr hids_report input_report_ref_2 = {
    .id = 0x02,
    .type = static_cast<uint8_t>(HidReportType::Input),
};

constexpr hids_report input_report_ref_3 = {
    .id = 0x03,
    .type = static_cast<uint8_t>(HidReportType::Input),
};

constexpr size_t kReportLen1 = 1;
constexpr size_t kReportLen2 = 5;
constexpr size_t kReportLen3 = 8;

uint8_t ctrl_point;

// Composite 3-report HID descriptor:
//   Report ID 1 (1 byte): Consumer Control (Volume Up/Down, Mute, Play/Pause, Next/Prev, AC Back/Forward)
//   Report ID 2 (5 bytes): Mouse & Scroll Wheel [buttons, dx, dy, wheel, ac_pan]
//   Report ID 3 (8 bytes): Standard Keyboard [modifiers, reserved, key0..key5]
constexpr auto report_map = std::to_array<uint8_t>({
    // ── Report ID 1: Consumer Control ──────────────────────────────
    0x05, 0x0c,       // Usage Page (Consumer)
    0x09, 0x01,       // Usage (Consumer Control)
    0xa1, 0x01,       // Collection (Application)
    0x85, 0x01,       //   Report ID (1)
    0x15, 0x00,       //   Logical Minimum (0)
    0x25, 0x01,       //   Logical Maximum (1)
    0x75, 0x01,       //   Report Size (1)
    0x95, 0x08,       //   Report Count (8)
    0x09, 0xe9,       //   Usage (Volume Increment)         - 0x01
    0x09, 0xea,       //   Usage (Volume Decrement)         - 0x02
    0x09, 0xe2,       //   Usage (Mute)                     - 0x04
    0x09, 0xcd,       //   Usage (Play/Pause)               - 0x08
    0x09, 0xb5,       //   Usage (Scan Next Track)          - 0x10
    0x09, 0xb6,       //   Usage (Scan Previous Track)      - 0x20
    0x0a, 0x24, 0x02, //   Usage (AC Back)                  - 0x40
    0x0a, 0x25, 0x02, //   Usage (AC Forward)               - 0x80
    0x81, 0x02,       //   Input (Data, Variable, Absolute)
    0xc0,             // End Collection

    // ── Report ID 2: Mouse Buttons + Vertical Wheel + Horizontal Pan ─
    0x05, 0x01,       // Usage Page (Generic Desktop)
    0x09, 0x02,       // Usage (Mouse)
    0xa1, 0x01,       // Collection (Application)
    0x85, 0x02,       //   Report ID (2)
    0x09, 0x01,       //   Usage (Pointer)
    0xa1, 0x00,       //   Collection (Physical)
    0x05, 0x09,       //     Usage Page (Buttons)
    0x19, 0x01,       //     Usage Minimum (Button 1 - Left)
    0x29, 0x03,       //     Usage Maximum (Button 3 - Middle)
    0x15, 0x00,       //     Logical Minimum (0)
    0x25, 0x01,       //     Logical Maximum (1)
    0x95, 0x03,       //     Report Count (3)
    0x75, 0x01,       //     Report Size (1)
    0x81, 0x02,       //     Input (Data, Variable, Absolute)
    0x95, 0x01,       //     Report Count (1)
    0x75, 0x05,       //     Report Size (5) - padding
    0x81, 0x03,       //     Input (Constant, Variable, Absolute)
    0x05, 0x01,       //     Usage Page (Generic Desktop)
    0x09, 0x30,       //     Usage (X)
    0x09, 0x31,       //     Usage (Y)
    0x09, 0x38,       //     Usage (Wheel)
    0x15, 0x81,       //     Logical Minimum (-127)
    0x25, 0x7f,       //     Logical Maximum (127)
    0x75, 0x08,       //     Report Size (8)
    0x95, 0x03,       //     Report Count (3)
    0x81, 0x06,       //     Input (Data, Variable, Relative)
    0x05, 0x0c,       //     Usage Page (Consumer)
    0x0a, 0x38, 0x02, //     Usage (AC Pan)
    0x95, 0x01,       //     Report Count (1)
    0x81, 0x06,       //     Input (Data, Variable, Relative)
    0xc0,             //   End Collection (Physical)
    0xc0,             // End Collection (Application)

    // ── Report ID 3: Standard Keyboard & Shortcuts ─────────────────
    0x05, 0x01,       // Usage Page (Generic Desktop)
    0x09, 0x06,       // Usage (Keyboard)
    0xa1, 0x01,       // Collection (Application)
    0x85, 0x03,       //   Report ID (3)
    0x05, 0x07,       //   Usage Page (Keyboard/Keypad)
    0x19, 0xe0,       //   Usage Minimum (Left Control)
    0x29, 0xe7,       //   Usage Maximum (Right GUI)
    0x15, 0x00,       //   Logical Minimum (0)
    0x25, 0x01,       //   Logical Maximum (1)
    0x75, 0x01,       //   Report Size (1)
    0x95, 0x08,       //   Report Count (8)
    0x81, 0x02,       //   Input (Data, Variable, Absolute) - Modifiers
    0x95, 0x01,       //   Report Count (1)
    0x75, 0x08,       //   Report Size (8)
    0x81, 0x03,       //   Input (Constant, Variable, Absolute) - Reserved
    0x95, 0x06,       //   Report Count (6)
    0x75, 0x08,       //   Report Size (8)
    0x15, 0x00,       //   Logical Minimum (0)
    0x25, 0x65,       //   Logical Maximum (101)
    0x05, 0x07,       //   Usage Page (Keyboard/Keypad)
    0x19, 0x00,       //   Usage Minimum (0x00)
    0x29, 0x65,       //   Usage Maximum (0x65)
    0x81, 0x00,       //   Input (Data, Array, Absolute) - Keycodes
    0xc0              // End Collection
});

// ─── GATT Callbacks ──────────────────────────────────────────────

ssize_t read_info(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                  void *buf, uint16_t len, uint16_t offset)
{
    return bt_gatt_attr_read(conn, attr, buf, len, offset, attr->user_data,
                             sizeof(hids_info));
}

ssize_t read_report_map(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                        void *buf, uint16_t len, uint16_t offset)
{
    return bt_gatt_attr_read(conn, attr, buf, len, offset, report_map.data(),
                             report_map.size());
}

ssize_t read_report_ref(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                        void *buf, uint16_t len, uint16_t offset)
{
    return bt_gatt_attr_read(conn, attr, buf, len, offset, attr->user_data,
                             sizeof(hids_report));
}

ssize_t read_input_report(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                          void *buf, uint16_t len, uint16_t offset)
{
    static constexpr std::array<uint8_t, kReportLen3> empty_report = {};
    const size_t report_len = attr->user_data ? *static_cast<const size_t *>(attr->user_data) : kReportLen1;
    return bt_gatt_attr_read(conn, attr, buf, len, offset, empty_report.data(), report_len);
}

ssize_t write_ctrl_point(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                         const void *buf, uint16_t len, uint16_t offset, uint8_t flags)
{
    auto *value = static_cast<uint8_t *>(attr->user_data);
    if (offset + len > sizeof(ctrl_point)) {
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
    }
    memcpy(value + offset, buf, len);
    return len;
}

#if CONFIG_SAMPLE_BT_USE_AUTHENTICATION
#define SAMPLE_BT_PERM_READ BT_GATT_PERM_READ_AUTHEN
#define SAMPLE_BT_PERM_WRITE BT_GATT_PERM_WRITE_AUTHEN
#else
#define SAMPLE_BT_PERM_READ BT_GATT_PERM_READ_ENCRYPT
#define SAMPLE_BT_PERM_WRITE BT_GATT_PERM_WRITE_ENCRYPT
#endif


// ─── Custom Config Service Internals ──────────────────────────────

K_MUTEX_DEFINE(config_mutex);

static AllSlotMappings g_slot_mappings = {
    kDefaultConfigMappings,
    kDefaultConfigMappings,
    kDefaultConfigMappings,
};

static ssize_t read_config(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                           void *buf, uint16_t len, uint16_t offset)
{
    if (get_active_profile_id() < kNumHidSlots) {
        return BT_GATT_ERR(BT_ATT_ERR_NOT_SUPPORTED);
    }
    k_mutex_lock(&config_mutex, K_FOREVER);
    const AllSlotMappings snapshot = g_slot_mappings;
    k_mutex_unlock(&config_mutex);
    return bt_gatt_attr_read(conn, attr, buf, len, offset, snapshot.data(), sizeof(snapshot));
}

static ssize_t write_config(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                            const void *buf, uint16_t len, uint16_t offset, uint8_t flags)
{
    ARG_UNUSED(conn);
    ARG_UNUSED(attr);
    ARG_UNUSED(flags);
    if (get_active_profile_id() < kNumHidSlots) {
        return BT_GATT_ERR(BT_ATT_ERR_NOT_SUPPORTED);
    }
    if (offset + len > sizeof(g_slot_mappings)) {
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
    }
    const auto *bytes = static_cast<const uint8_t *>(buf);
    for (uint16_t i = 0; i < len; ++i) {
        if (!is_valid_hid_key(bytes[i])) {
            return BT_GATT_ERR(BT_ATT_ERR_VALUE_NOT_ALLOWED);
        }
    }

    k_mutex_lock(&config_mutex, K_FOREVER);
    if (offset == 0 && len == sizeof(ConfigMappings)) {
        // Single-slot 7-byte write updates slot 0
        memcpy(&g_slot_mappings[0], buf, sizeof(ConfigMappings));
    } else if (offset == 0 && len == kLegacyAllSlotMappingsSize) {
        // Legacy 15-byte write (3 x 5 bytes): preserve default secondary button mappings
        for (size_t s = 0; s < kNumHidSlots; ++s) {
            g_slot_mappings[s] = unpack_legacy_mapping(bytes + (s * kLegacyMappingSize));
        }
    } else if (offset == 0 && len == kLegacyMappingSize) {
        // Legacy 5-byte write updates slot 0 and preserves default secondary button mappings
        g_slot_mappings[0] = unpack_legacy_mapping(bytes);
    } else {
        memcpy(reinterpret_cast<uint8_t *>(g_slot_mappings.data()) + offset, buf, len);
    }
    const AllSlotMappings updated = g_slot_mappings;
    k_mutex_unlock(&config_mutex);
    
    // Persist to NVS immediately
    if (IS_ENABLED(CONFIG_SETTINGS)) {
        settings_save_one("remote/config/mappings", updated.data(), sizeof(updated));
    }
    
    printk("Config: slot mappings updated via BLE and saved to NVS (%u bytes)\n", len);
    return len;
}

#define BT_UUID_REMOTE_SERVICE_VAL \
    BT_UUID_128_ENCODE(0x9e4571a5, 0xb7d9, 0x4daa, 0xa3e5, 0xa8f086007a2b)

#define BT_UUID_REMOTE_CONFIG_CHRC_VAL \
    BT_UUID_128_ENCODE(0xa427616a, 0xed05, 0x499d, 0x8072, 0x5f329ed64b3b)

static struct bt_uuid_128 remote_svc_uuid = BT_UUID_INIT_128(BT_UUID_REMOTE_SERVICE_VAL);
static struct bt_uuid_128 config_chrc_uuid = BT_UUID_INIT_128(BT_UUID_REMOTE_CONFIG_CHRC_VAL);


} // namespace

BT_GATT_SERVICE_DEFINE(hog_svc,
    BT_GATT_PRIMARY_SERVICE(BT_UUID_HIDS),
    BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_INFO, BT_GATT_CHRC_READ,
                           BT_GATT_PERM_READ, read_info, nullptr, const_cast<hids_info*>(&info)),
    BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_REPORT_MAP, BT_GATT_CHRC_READ,
                           BT_GATT_PERM_READ, read_report_map, nullptr, nullptr),
    // Report ID 1: Consumer Control (attrs[6])
    BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_REPORT,
                           BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
                           SAMPLE_BT_PERM_READ,
                           read_input_report, nullptr, const_cast<size_t*>(&kReportLen1)),
    BT_GATT_CCC(nullptr, SAMPLE_BT_PERM_READ | SAMPLE_BT_PERM_WRITE),
    BT_GATT_DESCRIPTOR(BT_UUID_HIDS_REPORT_REF, BT_GATT_PERM_READ,
                       read_report_ref, nullptr, const_cast<hids_report*>(&input_report_ref_1)),
    // Report ID 2: Mouse & Scrolling (attrs[10])
    BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_REPORT,
                           BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
                           SAMPLE_BT_PERM_READ,
                           read_input_report, nullptr, const_cast<size_t*>(&kReportLen2)),
    BT_GATT_CCC(nullptr, SAMPLE_BT_PERM_READ | SAMPLE_BT_PERM_WRITE),
    BT_GATT_DESCRIPTOR(BT_UUID_HIDS_REPORT_REF, BT_GATT_PERM_READ,
                       read_report_ref, nullptr, const_cast<hids_report*>(&input_report_ref_2)),
    // Report ID 3: Standard Keyboard & Shortcuts (attrs[14])
    BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_REPORT,
                           BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
                           SAMPLE_BT_PERM_READ,
                           read_input_report, nullptr, const_cast<size_t*>(&kReportLen3)),
    BT_GATT_CCC(nullptr, SAMPLE_BT_PERM_READ | SAMPLE_BT_PERM_WRITE),
    BT_GATT_DESCRIPTOR(BT_UUID_HIDS_REPORT_REF, BT_GATT_PERM_READ,
                       read_report_ref, nullptr, const_cast<hids_report*>(&input_report_ref_3)),
    BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_CTRL_POINT,
                           BT_GATT_CHRC_WRITE_WITHOUT_RESP,
                           BT_GATT_PERM_WRITE,
                           nullptr, write_ctrl_point, &ctrl_point),
);

BT_GATT_SERVICE_DEFINE(remote_svc,
    BT_GATT_PRIMARY_SERVICE(&remote_svc_uuid),
    BT_GATT_CHARACTERISTIC(&config_chrc_uuid.uuid,
                           BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE,
                           BT_GATT_PERM_READ | BT_GATT_PERM_WRITE,
                           read_config, write_config, &g_slot_mappings),
);

// ─── Button Handler (Thread-based state machine) ─────────────────


// ─── Button Engine (Interrupt-driven) ───────────────

namespace {

const struct gpio_dt_spec btn_p5 = GPIO_DT_SPEC_GET(DT_NODELABEL(button0), gpios);
const struct gpio_dt_spec btn_p4 = GPIO_DT_SPEC_GET(DT_NODELABEL(button1), gpios);
const struct gpio_dt_spec btn_p7 = GPIO_DT_SPEC_GET(DT_NODELABEL(button2), gpios);
const struct device *const qdec_dev = DEVICE_DT_GET(DT_NODELABEL(qdec0));

// ─── Connection tracking (per-slot: 0..2 HID, 3 Config) ───────────
static constexpr uint8_t kTotalSlots = 4;
K_MUTEX_DEFINE(conn_mutex);
struct bt_conn *slot_conns[kTotalSlots] = {nullptr, nullptr, nullptr, nullptr};
static bool qdec_trigger_active = false;

// ─── Button & Encoder synchronization ───────────────────────────
K_SEM_DEFINE(btn_sem, 0, 1);
K_SEM_DEFINE(sec_btn_sem, 0, 1);
K_SEM_DEFINE(enc_btn_sem, 0, 1);
K_SEM_DEFINE(enc_rot_sem, 0, 1);
static struct gpio_callback btn_p5_cb_data;
static struct gpio_callback btn_p4_cb_data;
static struct gpio_callback btn_p7_cb_data;

// ─── Multi-report HID dispatch ────────────────────────────────────
enum class ReportTarget : uint8_t { Consumer = 1, Mouse = 2, Keyboard = 3 };

struct DecodedHidAction {
    ReportTarget target;
    uint8_t consumer_bits = 0;
    uint8_t mouse_buttons = 0;
    int8_t wheel = 0;
    int8_t pan = 0;
    uint8_t modifiers = 0;
    uint8_t keycode = 0;
};

static constexpr uint8_t kModLCtrl  = 0x01;
static constexpr uint8_t kModLShift = 0x02;
static constexpr uint8_t kModLAlt   = 0x04;
static constexpr uint8_t kModLGui   = 0x08;

static constexpr DecodedHidAction decode_hid_action(HidKey key) noexcept
{
    switch (key) {
        // Mouse & Scrolling (Report ID 2)
        case HidKey::ScrollUp:         return { ReportTarget::Mouse, 0, 0x00,  1,  0, 0, 0 };
        case HidKey::ScrollDown:       return { ReportTarget::Mouse, 0, 0x00, -1,  0, 0, 0 };
        case HidKey::ScrollUpFast:     return { ReportTarget::Mouse, 0, 0x00,  3,  0, 0, 0 };
        case HidKey::ScrollDownFast:   return { ReportTarget::Mouse, 0, 0x00, -3,  0, 0, 0 };
        case HidKey::ScrollLeft:       return { ReportTarget::Mouse, 0, 0x00,  0, -1, 0, 0 };
        case HidKey::ScrollRight:      return { ReportTarget::Mouse, 0, 0x00,  0,  1, 0, 0 };
        case HidKey::MouseLeftClick:   return { ReportTarget::Mouse, 0, 0x01,  0,  0, 0, 0 };
        case HidKey::MouseRightClick:  return { ReportTarget::Mouse, 0, 0x02,  0,  0, 0, 0 };
        case HidKey::MouseMiddleClick: return { ReportTarget::Mouse, 0, 0x04,  0,  0, 0, 0 };

        // Keyboard Navigation & Function Keys (Report ID 3)
        case HidKey::KeySpace:         return { ReportTarget::Keyboard, 0, 0, 0, 0, 0, 0x2C };
        case HidKey::KeyEnter:         return { ReportTarget::Keyboard, 0, 0, 0, 0, 0, 0x28 };
        case HidKey::KeyEscape:        return { ReportTarget::Keyboard, 0, 0, 0, 0, 0, 0x29 };
        case HidKey::KeyTab:           return { ReportTarget::Keyboard, 0, 0, 0, 0, 0, 0x2B };
        case HidKey::KeyBackspace:     return { ReportTarget::Keyboard, 0, 0, 0, 0, 0, 0x2A };
        case HidKey::KeyUp:            return { ReportTarget::Keyboard, 0, 0, 0, 0, 0, 0x52 };
        case HidKey::KeyDown:          return { ReportTarget::Keyboard, 0, 0, 0, 0, 0, 0x51 };
        case HidKey::KeyLeft:          return { ReportTarget::Keyboard, 0, 0, 0, 0, 0, 0x50 };
        case HidKey::KeyRight:         return { ReportTarget::Keyboard, 0, 0, 0, 0, 0, 0x4F };
        case HidKey::KeyPageUp:        return { ReportTarget::Keyboard, 0, 0, 0, 0, 0, 0x4B };
        case HidKey::KeyPageDown:      return { ReportTarget::Keyboard, 0, 0, 0, 0, 0, 0x4E };
        case HidKey::KeyHome:          return { ReportTarget::Keyboard, 0, 0, 0, 0, 0, 0x4A };
        case HidKey::KeyEnd:           return { ReportTarget::Keyboard, 0, 0, 0, 0, 0, 0x4D };
        case HidKey::KeyF5:            return { ReportTarget::Keyboard, 0, 0, 0, 0, 0, 0x3E };
        case HidKey::KeyFullscreen:    return { ReportTarget::Keyboard, 0, 0, 0, 0, 0, 0x09 };
        case HidKey::KeyPrintScreen:   return { ReportTarget::Keyboard, 0, 0, 0, 0, 0, 0x46 };

        // Keyboard Shortcuts (Report ID 3 with Modifiers)
        case HidKey::ShortcutCopy:        return { ReportTarget::Keyboard, 0, 0, 0, 0, kModLCtrl,              0x06 };
        case HidKey::ShortcutPaste:       return { ReportTarget::Keyboard, 0, 0, 0, 0, kModLCtrl,              0x19 };
        case HidKey::ShortcutCut:         return { ReportTarget::Keyboard, 0, 0, 0, 0, kModLCtrl,              0x1B };
        case HidKey::ShortcutUndo:        return { ReportTarget::Keyboard, 0, 0, 0, 0, kModLCtrl,              0x1D };
        case HidKey::ShortcutRedo:        return { ReportTarget::Keyboard, 0, 0, 0, 0, kModLCtrl,              0x1C };
        case HidKey::ShortcutSelectAll:   return { ReportTarget::Keyboard, 0, 0, 0, 0, kModLCtrl,              0x04 };
        case HidKey::ShortcutSave:        return { ReportTarget::Keyboard, 0, 0, 0, 0, kModLCtrl,              0x16 };
        case HidKey::ShortcutFind:        return { ReportTarget::Keyboard, 0, 0, 0, 0, kModLCtrl,              0x09 };
        case HidKey::ShortcutZoomIn:      return { ReportTarget::Keyboard, 0, 0, 0, 0, kModLCtrl,              0x2E };
        case HidKey::ShortcutZoomOut:     return { ReportTarget::Keyboard, 0, 0, 0, 0, kModLCtrl,              0x2D };
        case HidKey::ShortcutZoomReset:   return { ReportTarget::Keyboard, 0, 0, 0, 0, kModLCtrl,              0x27 };
        case HidKey::ShortcutNewTab:      return { ReportTarget::Keyboard, 0, 0, 0, 0, kModLCtrl,              0x17 };
        case HidKey::ShortcutCloseTab:    return { ReportTarget::Keyboard, 0, 0, 0, 0, kModLCtrl,              0x1A };
        case HidKey::ShortcutReopenTab:   return { ReportTarget::Keyboard, 0, 0, 0, 0, kModLCtrl | kModLShift, 0x17 };
        case HidKey::ShortcutNextTab:     return { ReportTarget::Keyboard, 0, 0, 0, 0, kModLCtrl,              0x2B };
        case HidKey::ShortcutPrevTab:     return { ReportTarget::Keyboard, 0, 0, 0, 0, kModLCtrl | kModLShift, 0x2B };
        case HidKey::ShortcutAltTab:      return { ReportTarget::Keyboard, 0, 0, 0, 0, kModLAlt,               0x2B };
        case HidKey::ShortcutShowDesktop: return { ReportTarget::Keyboard, 0, 0, 0, 0, kModLGui,               0x07 };
        case HidKey::ShortcutLockScreen:  return { ReportTarget::Keyboard, 0, 0, 0, 0, kModLGui,               0x0F };
        case HidKey::ShortcutSnippingTool:return { ReportTarget::Keyboard, 0, 0, 0, 0, kModLGui | kModLShift,  0x16 };

        // Consumer Control (Report ID 1)
        default: return { ReportTarget::Consumer, static_cast<uint8_t>(key), 0, 0, 0, 0, 0 };
    }
}

K_MUTEX_DEFINE(report_mutex);
static uint8_t active_report_mask = 0;

// ─── HID report (only sends to the active HID slot if connected) ─
static void send_hid_report(uint8_t key_code)
{
    const uint8_t slot = get_active_profile_id();
    if (slot >= kNumHidSlots) {
        return;
    }

    struct bt_conn *conn = nullptr;
    k_mutex_lock(&conn_mutex, K_FOREVER);
    if (slot_conns[slot]) {
        conn = bt_conn_ref(slot_conns[slot]);
    }
    k_mutex_unlock(&conn_mutex);

    if (!conn) {
        printk("HID: slot %u not connected (drop 0x%02x)\n", slot, key_code);
        return;
    }

    auto notify_attr = [&](size_t attr_idx, const void *data, uint16_t len) {
        int err = bt_gatt_notify(conn, &hog_svc.attrs[attr_idx], data, len);
        if (err) {
            printk("HID: notify err %d (slot %u, attr %zu)\n", err, slot, attr_idx);
        }
    };

    if (key_code == 0x00) {
        k_mutex_lock(&report_mutex, K_FOREVER);
        const uint8_t mask = active_report_mask;
        active_report_mask = 0;
        k_mutex_unlock(&report_mutex);

        if (mask & BIT(0)) {
            const uint8_t zero = 0x00;
            notify_attr(6, &zero, sizeof(zero));
        }
        if (mask & BIT(1)) {
            const std::array<uint8_t, 5> zero_mouse = {};
            notify_attr(10, zero_mouse.data(), zero_mouse.size());
        }
        if (mask & BIT(2)) {
            const std::array<uint8_t, 8> zero_kbd = {};
            notify_attr(14, zero_kbd.data(), zero_kbd.size());
        }
        bt_conn_unref(conn);
        return;
    }

    printk("HID: notify key 0x%02x (slot %u)\n", key_code, slot);
    const DecodedHidAction action = decode_hid_action(static_cast<HidKey>(key_code));
    switch (action.target) {
        case ReportTarget::Consumer: {
            k_mutex_lock(&report_mutex, K_FOREVER);
            active_report_mask |= BIT(0);
            k_mutex_unlock(&report_mutex);
            notify_attr(6, &action.consumer_bits, sizeof(action.consumer_bits));
            break;
        }
        case ReportTarget::Mouse: {
            if (action.mouse_buttons != 0) {
                k_mutex_lock(&report_mutex, K_FOREVER);
                active_report_mask |= BIT(1);
                k_mutex_unlock(&report_mutex);
            }
            const std::array<uint8_t, 5> report = {
                action.mouse_buttons,
                0x00,
                0x00,
                static_cast<uint8_t>(action.wheel),
                static_cast<uint8_t>(action.pan),
            };
            notify_attr(10, report.data(), report.size());
            break;
        }
        case ReportTarget::Keyboard: {
            k_mutex_lock(&report_mutex, K_FOREVER);
            active_report_mask |= BIT(2);
            k_mutex_unlock(&report_mutex);
            const std::array<uint8_t, 8> report = {
                action.modifiers, 0x00, action.keycode, 0x00, 0x00, 0x00, 0x00, 0x00
            };
            notify_attr(14, report.data(), report.size());
            break;
        }
    }

    bt_conn_unref(conn);
}

struct ZephyrHal {
    static uint64_t get_time_ms() {
        return k_uptime_get();
    }
    static void sleep_ms(int ms) {
        k_msleep(ms);
    }
    static bool is_button_held() {
        return gpio_pin_get_dt(&btn_p5) > 0;
    }
    static bool is_secondary_button_held() {
        return gpio_pin_get_dt(&btn_p4) > 0;
    }
    static bool is_encoder_button_held() {
        return gpio_pin_get_dt(&btn_p7) > 0;
    }
    static void send_hid_report(uint8_t key_bits) {
        ::remote::send_hid_report(key_bits);
    }
    static void buzzer_beep() {
        ::buzzer_beep();
    }
    static void buzzer_long_beep() {
        ::buzzer_long_beep();
    }
    static void led_set_trigger_active(bool active) {
        ::led_set_trigger_active(active);
    }
    static void on_profile_switch() {
        ::on_profile_switch_request();
    }
    static void clear_bonds() {
        ::clear_bonds();
    }
    static uint8_t get_active_profile() {
        return get_active_profile_id();
    }
    static ConfigMappings get_config_mappings() {
        return HidService::get_slot_mappings(get_active_profile_id());
    }
};

using Engine = GestureEngine<ZephyrHal>;

// ─── QDEC Sensor Trigger (ISR-safe handoff to thread) ───────────
static void qdec_trigger_handler(const struct device *dev, const struct sensor_trigger *trigger)
{
    ARG_UNUSED(dev);
    ARG_UNUSED(trigger);
    k_sem_give(&enc_rot_sem);
}

static struct sensor_trigger qdec_trig = {
    .type = SENSOR_TRIG_DATA_READY,
    .chan = SENSOR_CHAN_ROTATION,
};

static void button_pressed_isr(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    ARG_UNUSED(cb);
    if (dev == btn_p7.port && (pins & BIT(btn_p7.pin))) {
        k_sem_give(&enc_btn_sem);
    }
    if (dev == btn_p5.port && (pins & BIT(btn_p5.pin))) {
        k_sem_give(&btn_sem);
    }
    if (dev == btn_p4.port && (pins & BIT(btn_p4.pin))) {
        k_sem_give(&sec_btn_sem);
    }
}

// ─── Button thread (sleeping) ───────────────────────────────────

K_THREAD_STACK_DEFINE(btn_stack, 2048);
struct k_thread btn_thread_data;

void button_thread(void *, void *, void *)
{
    printk("Button thread started (interrupt-driven)\n");

    while (true) {
        printk("Zzz... Sleeping\n");
        k_sem_take(&btn_sem, K_FOREVER);
        printk("Woke up!\n");
        
        Engine::on_main_button_wake();

        k_sem_reset(&btn_sem);
        printk("BTN: Ready\n");
    }
}

// ─── Secondary button thread (Trigger 2 / D4) ────────────────────

K_THREAD_STACK_DEFINE(sec_btn_stack, 1024);
struct k_thread sec_btn_thread_data;

void secondary_button_thread(void *, void *, void *)
{
    printk("Secondary button thread started (interrupt-driven)\n");

    while (true) {
        printk("Zzz... Sleeping (SecBtn)\n");
        k_sem_take(&sec_btn_sem, K_FOREVER);
        printk("Woke up (SecBtn)!\n");
        Engine::on_secondary_button_wake();
        k_sem_reset(&sec_btn_sem);
        printk("SEC_BTN: Ready\n");
    }
}

// ─── Encoder button thread (long-press detection) ───────────────

K_THREAD_STACK_DEFINE(enc_btn_stack, 1024);
struct k_thread enc_btn_thread_data;

void encoder_button_thread(void *, void *, void *)
{
    printk("Encoder button thread started\n");

    while (true) {
        printk("Zzz... Sleeping (EncBtn)\n");
        k_sem_take(&enc_btn_sem, K_FOREVER);
        printk("Woke up (EncBtn)!\n");
        Engine::on_encoder_button_press();
        k_sem_reset(&enc_btn_sem);
    }
}

// ─── Encoder rotation thread (deferred from QDEC ISR) ──────────

K_THREAD_STACK_DEFINE(enc_rot_stack, 1024);
struct k_thread enc_rot_thread_data;

void encoder_rot_thread(void *, void *, void *)
{
    printk("Encoder rotation thread started\n");

    while (true) {
        k_sem_take(&enc_rot_sem, K_FOREVER);
        if (!device_is_ready(qdec_dev)) {
            continue;
        }
        struct sensor_value val;
        if (sensor_sample_fetch(qdec_dev) == 0 &&
            sensor_channel_get(qdec_dev, SENSOR_CHAN_ROTATION, &val) == 0 &&
            val.val1 != 0) {
            printk("Woke up (Encoder)! (%d deg)\n", val.val1);
            Engine::on_encoder_rotate(val.val1);
            printk("Zzz... Sleeping (Encoder)\n");
        }
    }
}

} // namespace

// ─── Connection tracking (called from main.cpp) ─────────────────

extern "C" bool hid_is_slot_connected(uint8_t slot)
{
    if (slot >= kTotalSlots) {
        return false;
    }
    k_mutex_lock(&conn_mutex, K_FOREVER);
    const bool connected = (slot_conns[slot] != nullptr);
    k_mutex_unlock(&conn_mutex);
    return connected;
}

extern "C" struct bt_conn *hid_get_slot_conn_ref(uint8_t slot)
{
    if (slot >= kTotalSlots) {
        return nullptr;
    }
    struct bt_conn *conn = nullptr;
    k_mutex_lock(&conn_mutex, K_FOREVER);
    if (slot_conns[slot]) {
        conn = bt_conn_ref(slot_conns[slot]);
    }
    k_mutex_unlock(&conn_mutex);
    return conn;
}

extern "C" void hid_update_qdec_trigger()
{
    Engine::reset();
    if (!device_is_ready(qdec_dev) || qdec_trigger_active) {
        return;
    }
    int err = sensor_trigger_set(qdec_dev, &qdec_trig, qdec_trigger_handler);
    if (err) {
        printk("QDEC: failed to set trigger (err %d)\n", err);
    } else {
        qdec_trigger_active = true;
        printk("QDEC: trigger registered\n");
    }
}

extern "C" void hid_set_slot_conn(uint8_t slot, struct bt_conn *conn)
{
    if (slot >= kTotalSlots || !conn) {
        return;
    }
    struct bt_conn *old_conn = nullptr;
    k_mutex_lock(&conn_mutex, K_FOREVER);
    if (slot_conns[slot] != conn) {
        old_conn = slot_conns[slot];
        slot_conns[slot] = bt_conn_ref(conn);
    }
    k_mutex_unlock(&conn_mutex);

    if (old_conn) {
        bt_conn_unref(old_conn);
    }
    printk("HID: slot %u conn set\n", slot);
    hid_update_qdec_trigger();
}

extern "C" void hid_disconnect_slot(uint8_t slot)
{
    if (slot >= kTotalSlots) {
        return;
    }
    struct bt_conn *conn = nullptr;
    k_mutex_lock(&conn_mutex, K_FOREVER);
    if (slot_conns[slot]) {
        conn = bt_conn_ref(slot_conns[slot]);
    }
    k_mutex_unlock(&conn_mutex);

    if (conn) {
        printk("HID: disconnecting slot %u...\n", slot);
        bt_conn_disconnect(conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
        bt_conn_unref(conn);
    }
}

extern "C" bool hid_clear_slot_conn(uint8_t slot, struct bt_conn *conn)
{
    if (slot >= kTotalSlots) {
        return false;
    }
    struct bt_conn *old_conn = nullptr;
    k_mutex_lock(&conn_mutex, K_FOREVER);
    if (slot_conns[slot] && (!conn || slot_conns[slot] == conn)) {
        old_conn = slot_conns[slot];
        slot_conns[slot] = nullptr;
    }
    k_mutex_unlock(&conn_mutex);

    if (old_conn) {
        bt_conn_unref(old_conn);
        printk("HID: slot %u conn cleared\n", slot);
        hid_update_qdec_trigger();
        return true;
    }
    return false;
}

// ─── Public API ──────────────────────────────────────────────────

void HidService::init()
{
    k_thread_create(&btn_thread_data, btn_stack,
                    K_THREAD_STACK_SIZEOF(btn_stack),
                    button_thread, nullptr, nullptr, nullptr,
                    7, 0, K_NO_WAIT);
    k_thread_name_set(&btn_thread_data, "btn");

    k_thread_create(&sec_btn_thread_data, sec_btn_stack,
                    K_THREAD_STACK_SIZEOF(sec_btn_stack),
                    secondary_button_thread, nullptr, nullptr, nullptr,
                    7, 0, K_NO_WAIT);
    k_thread_name_set(&sec_btn_thread_data, "sec_btn");

    k_thread_create(&enc_btn_thread_data, enc_btn_stack,
                    K_THREAD_STACK_SIZEOF(enc_btn_stack),
                    encoder_button_thread, nullptr, nullptr, nullptr,
                    7, 0, K_NO_WAIT);
    k_thread_name_set(&enc_btn_thread_data, "enc_btn");

    k_thread_create(&enc_rot_thread_data, enc_rot_stack,
                    K_THREAD_STACK_SIZEOF(enc_rot_stack),
                    encoder_rot_thread, nullptr, nullptr, nullptr,
                    7, 0, K_NO_WAIT);
    k_thread_name_set(&enc_rot_thread_data, "enc_rot");

    if (!device_is_ready(qdec_dev)) {
        printk("QDEC sensor not ready\n");
    } else {
        int err = sensor_trigger_set(qdec_dev, &qdec_trig, qdec_trigger_handler);
        if (err) {
            printk("QDEC: failed to set trigger (err %d)\n", err);
        } else {
            qdec_trigger_active = true;
            printk("QDEC: trigger registered\n");
        }
    }
}

void HidService::send_key(HidKey key)
{
    send_hid_report(static_cast<uint8_t>(key));
    k_msleep(100);
    send_hid_report(0x00);
}

void HidService::send_key_down(HidKey key)
{
    send_hid_report(static_cast<uint8_t>(key));
}

void HidService::send_key_up()
{
    send_hid_report(0x00);
}

void HidService::set_slot_mappings(uint8_t slot, const ConfigMappings& mappings)
{
    const uint8_t target_slot = (slot < kNumHidSlots) ? slot : 0;
    k_mutex_lock(&config_mutex, K_FOREVER);
    g_slot_mappings[target_slot] = mappings;
    k_mutex_unlock(&config_mutex);
}

ConfigMappings HidService::get_slot_mappings(uint8_t slot)
{
    const uint8_t target_slot = (slot < kNumHidSlots) ? slot : 0;
    k_mutex_lock(&config_mutex, K_FOREVER);
    const ConfigMappings snapshot = g_slot_mappings[target_slot];
    k_mutex_unlock(&config_mutex);
    return snapshot;
}

void HidService::set_all_slot_mappings(const AllSlotMappings& mappings)
{
    k_mutex_lock(&config_mutex, K_FOREVER);
    g_slot_mappings = mappings;
    k_mutex_unlock(&config_mutex);
}

AllSlotMappings HidService::get_all_slot_mappings()
{
    k_mutex_lock(&config_mutex, K_FOREVER);
    const AllSlotMappings snapshot = g_slot_mappings;
    k_mutex_unlock(&config_mutex);
    return snapshot;
}

void HidService::set_config_mappings(const ConfigMappings& mappings)
{
    k_mutex_lock(&config_mutex, K_FOREVER);
    g_slot_mappings.fill(mappings);
    k_mutex_unlock(&config_mutex);
}

ConfigMappings HidService::get_config_mappings()
{
    return get_slot_mappings(get_active_profile_id());
}

void HidService::run_button_loop()
{
    if (!device_is_ready(btn_p5.port) || !device_is_ready(btn_p4.port) || !device_is_ready(btn_p7.port)) {
        printk("BTN: GPIO port(s) not ready\n");
        return;
    }

    gpio_pin_configure_dt(&btn_p5, GPIO_INPUT);
    gpio_pin_configure_dt(&btn_p4, GPIO_INPUT);
    gpio_pin_configure_dt(&btn_p7, GPIO_INPUT);

    gpio_pin_interrupt_configure_dt(&btn_p5, GPIO_INT_EDGE_TO_ACTIVE);
    gpio_pin_interrupt_configure_dt(&btn_p4, GPIO_INT_EDGE_TO_ACTIVE);
    gpio_pin_interrupt_configure_dt(&btn_p7, GPIO_INT_EDGE_TO_ACTIVE);

    gpio_init_callback(&btn_p5_cb_data, button_pressed_isr, BIT(btn_p5.pin));
    gpio_add_callback_dt(&btn_p5, &btn_p5_cb_data);

    gpio_init_callback(&btn_p4_cb_data, button_pressed_isr, BIT(btn_p4.pin));
    gpio_add_callback_dt(&btn_p4, &btn_p4_cb_data);

    gpio_init_callback(&btn_p7_cb_data, button_pressed_isr, BIT(btn_p7.pin));
    gpio_add_callback_dt(&btn_p7, &btn_p7_cb_data);

    printk("HID Ready: Press=Photo, Hold=Burst (interrupt-driven)\n");
    k_sleep(K_FOREVER);
}

} // namespace remote
