#include "hog.hpp"
#include "battery.hpp"
#include "profile_manager.hpp"

#include <zephyr/version.h>
#include <zephyr/types.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/kernel.h>
#include <zephyr/settings/settings.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pwm.h>
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
#include <zephyr/usb/usb_device.h>
#endif

#include <array>
#include <optional>
#include <string_view>
#include <cstdio>
#include <cstring>

static_assert(sizeof(bt_addr_le_t) == 7, "bt_addr_le_t must be 7 bytes");

#if KERNEL_VERSION_MAJOR >= 4
static constexpr uint32_t kAdvOptConn = BT_LE_ADV_OPT_CONN;
#else
static constexpr uint32_t kAdvOptConn = BT_LE_ADV_OPT_CONNECTABLE;
#endif

namespace remote {

class BuzzerController {
public:
    static void init() {
        if (!device_is_ready(pwm_buzzer.dev)) {
            printk("PWM buzzer device not ready\n");
        }
    }

    static void beep() {
        if (device_is_ready(pwm_buzzer.dev)) {
            printk("BEEP\n");
            uint32_t pulse = pwm_buzzer.period / 2;
            pwm_set_pulse_dt(&pwm_buzzer, pulse);
            k_sleep(K_MSEC(50));
            pwm_set_pulse_dt(&pwm_buzzer, 0);
        }
    }

    static void double_beep() {
        beep();
        k_sleep(K_MSEC(100));
        beep();
    }

    static void long_beep() {
        if (device_is_ready(pwm_buzzer.dev)) {
            uint32_t pulse = pwm_buzzer.period / 2;
            pwm_set_pulse_dt(&pwm_buzzer, pulse);
            k_sleep(K_MSEC(300));
            pwm_set_pulse_dt(&pwm_buzzer, 0);
        }
    }

    static void countdown_beep() {
        for (int i = 0; i < 3; ++i) {
            beep();
            k_sleep(K_MSEC(950));
        }
        long_beep();
    }

private:
    static inline const struct pwm_dt_spec pwm_buzzer = PWM_DT_SPEC_GET(DT_ALIAS(buzzer));
};

class LedController {
public:
    static void init() {
        init_pin(led_red, "Red");
    }

    static void set_connected(bool connected) {}
    static void set_advertising(bool advertising) {}

    static void set_trigger_active(bool active) {
        if (active) {
            printk("LED: 1 (ON)\n");
            if (device_is_ready(led_red.port)) gpio_pin_set_dt(&led_red, 1);
        } else {
            printk("LED: 0 (OFF)\n");
            if (device_is_ready(led_red.port)) gpio_pin_set_dt(&led_red, 0);
        }
    }

private:
    static void init_pin(const struct gpio_dt_spec &spec, const char *name) {
        if (device_is_ready(spec.port)) {
            gpio_pin_configure_dt(&spec, GPIO_OUTPUT_INACTIVE);
            printk("LED %s (pin %u) initialized to OFF\n", name, spec.pin);
        }
    }

    static inline const struct gpio_dt_spec led_red = GPIO_DT_SPEC_GET_OR(DT_ALIAS(led0), gpios, {0});
};

class PowerController {
public:
    static void init() {
        if (device_is_ready(vcc_rail.port)) {
            gpio_pin_configure_dt(&vcc_rail, GPIO_OUTPUT_ACTIVE);
            printk("VCC Rail (P0.13) enabled\n");
        } else {
            printk("VCC Rail device not ready\n");
        }
    }

private:
    static inline const struct gpio_dt_spec vcc_rail = GPIO_DT_SPEC_GET_OR(DT_ALIAS(vcc_rail), gpios, {0});
};

} // namespace remote



extern "C" void led_set_trigger_active(bool active) {
    remote::LedController::set_trigger_active(active);
}



extern "C" void buzzer_beep() {
    remote::BuzzerController::beep();
}

extern "C" void buzzer_double_beep() {
    remote::BuzzerController::double_beep();
}

extern "C" void buzzer_long_beep() {
    remote::BuzzerController::long_beep();
}

extern "C" void buzzer_countdown_beep() {
    remote::BuzzerController::countdown_beep();
}

// Connection tracking bridge (defined in hog.cpp)
extern "C" void hid_set_slot_conn(uint8_t slot, struct bt_conn *conn);
extern "C" void hid_clear_slot_conn(uint8_t slot, struct bt_conn *conn);
extern "C" void hid_disconnect_slot(uint8_t slot);
extern "C" bool hid_is_slot_connected(uint8_t slot);
extern "C" void hid_update_qdec_trigger();



namespace remote {

// ─── NVS-backed StorageHal for ProfileManager ──────────────────
struct NvsStorageHal {
    static bool store(std::string_view key, uint8_t value) {
        char key_str[64];
        std::snprintf(key_str, sizeof(key_str), "remote/%.*s", static_cast<int>(key.size()), key.data());
        int err = settings_save_one(key_str, &value, sizeof(value));
        if (err) {
            printk("NVS: save %s failed (%d)\n", key_str, err);
            return false;
        }
        return true;
    }

    static std::optional<uint8_t> load(std::string_view key) {
        return std::nullopt; // Handled by settings handler
    }

    static void trigger_save() {
        settings_save();
    }

    static bool store_addr(std::string_view key, const uint8_t* addr) {
        char key_str[64];
        std::snprintf(key_str, sizeof(key_str), "remote/%.*s", static_cast<int>(key.size()), key.data());
        if (ProfileManager<NvsStorageHal, 4>::is_zero_addr(addr)) {
            int err = settings_delete(key_str);
            return err == 0 || err == -ENOENT;
        }
        int err = settings_save_one(key_str, addr, 7);
        return err == 0;
    }

    static std::optional<std::array<uint8_t, 7>> load_addr(std::string_view key) {
        return std::nullopt; // Will be handled by settings_load() and a handler
    }
};

static ProfileManager<NvsStorageHal, 4> profile_mgr;

extern "C" uint8_t get_active_profile_id() {
    return remote::profile_mgr.active_slot();
}

// ─── Settings Handler ───────────────────────────────────────────
static int remote_settings_set(const char *name, size_t len, settings_read_cb read_cb, void *cb_arg) {
    const char *next;
    if (settings_name_steq(name, "profile/slot", &next) && !next) {
        uint8_t val;
        if (read_cb(cb_arg, &val, sizeof(val)) > 0) {
            profile_mgr.set_slot_no_store(val);
            return 0;
        }
    }
    
    // Handle profile/slotN/addr
    for (uint8_t i = 0; i < 4; ++i) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "profile/slot%u/addr", i);
        if (settings_name_steq(name, buf, &next) && !next) {
            uint8_t addr[7];
            if (read_cb(cb_arg, addr, sizeof(addr)) > 0) {
                profile_mgr.set_slot_addr(i, addr);
                return 0;
            }
        }
    }

    // Handle config/mappings
    if (settings_name_steq(name, "config/mappings", &next) && !next) {
        uint8_t raw[sizeof(AllSlotMappings)] = {};
        if (len > sizeof(raw)) {
            return -EINVAL;
        }
        ssize_t n = read_cb(cb_arg, raw, sizeof(raw));
        if (n == static_cast<ssize_t>(sizeof(AllSlotMappings))) {
            AllSlotMappings all_mappings;
            std::memcpy(all_mappings.data(), raw, sizeof(all_mappings));
            HidService::set_all_slot_mappings(all_mappings);
            return 0;
        } else if (n == static_cast<ssize_t>(kLegacyAllSlotMappingsSize)) {
            // Legacy 15-byte NVS entry (3 x 5 bytes): preserve default secondary button mappings
            AllSlotMappings all_mappings;
            for (size_t s = 0; s < kNumHidSlots; ++s) {
                all_mappings[s] = unpack_legacy_mapping(raw + (s * kLegacyMappingSize));
            }
            HidService::set_all_slot_mappings(all_mappings);
            return 0;
        } else if (n == static_cast<ssize_t>(sizeof(ConfigMappings))) {
            ConfigMappings single_mapping;
            std::memcpy(&single_mapping, raw, sizeof(single_mapping));
            HidService::set_config_mappings(single_mapping);
            return 0;
        } else if (n == static_cast<ssize_t>(kLegacyMappingSize)) {
            // Legacy 5-byte NVS entry: apply to all 3 HID slots with default secondary actions
            HidService::set_config_mappings(unpack_legacy_mapping(raw));
            return 0;
        }
    }
    return -ENOENT;
}

static struct settings_handler remote_conf = {
    .name = "remote",
    .h_set = remote_settings_set
};

// ─── Profile switch callback (forward declaration for hog.cpp) 
extern "C" void on_profile_switch_request();

// ─── Advertising state tracking ─────────────────────────────────
static volatile bool is_advertising = false;
static volatile bool suppress_adv_restart = false;
static int8_t last_adv_id = -1;      // Track which slot was last used for advertising

// Map HID Slots 0..2 to resettable Zephyr BT identities 1..3; Config Slot 3 uses BT_ID_DEFAULT (0)
static constexpr uint8_t slot_to_bt_id(uint8_t slot) noexcept {
    return (slot < 3) ? static_cast<uint8_t>(slot + 1) : BT_ID_DEFAULT;
}

static constexpr uint8_t bt_id_to_slot(uint8_t bt_id) noexcept {
    return (bt_id >= 1 && bt_id <= 3) ? static_cast<uint8_t>(bt_id - 1) : 3;
}

// ─── Advertising data (shared between start and watchdog) ───────
static constexpr uint8_t ad_flags[] = { BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR };
static constexpr uint8_t ad_uuid[] = {
    BT_UUID_16_ENCODE(BT_UUID_HIDS_VAL),
    BT_UUID_16_ENCODE(BT_UUID_BAS_VAL)
};
static constexpr std::array<std::string_view, 4> kSlotNames = {
    "Cam Remote Pro 1",
    "Cam Remote Pro 2",
    "Cam Remote Pro 3",
    "Cam Config",
};

static constexpr std::string_view slot_device_name(uint8_t slot) noexcept {
    return (slot < kSlotNames.size()) ? kSlotNames[slot] : kSlotNames[0];
}

static constexpr uint8_t sd_uuid[] = {
    0x2b, 0x7a, 0x00, 0x86, 0xf0, 0xa8, 0xe5, 0xa3,
    0xaa, 0x4d, 0xd9, 0xb7, 0xa5, 0x71, 0x45, 0x9e
};

// Advertising: Config Mode (Profile 3) — Advertises Custom Service and BAS
static constexpr std::string_view config_name = kSlotNames[3];
static const std::array<struct bt_data, 2> ad_config = {{
    { .type = BT_DATA_FLAGS, .data_len = sizeof(ad_flags), .data = ad_flags },
    { .type = BT_DATA_UUID128_ALL, .data_len = sizeof(sd_uuid), .data = sd_uuid },
}};

static const std::array<struct bt_data, 1> sd_config = {{
    { .type = BT_DATA_NAME_COMPLETE, .data_len = static_cast<uint8_t>(config_name.size()), .data = reinterpret_cast<const uint8_t*>(config_name.data()) },
}};

// ─── Advertising watchdog (forward declaration) ─────────────────
static void adv_watchdog_handler(struct k_work *work);
static struct k_work_delayable adv_watchdog_work;
constexpr int kAdvWatchdogIntervalSec = 5;

class BluetoothManager {
public:
    static int start() {
        k_work_init_delayable(&adv_watchdog_work, adv_watchdog_handler);

        int err = settings_subsys_init();
        if (err) {
            printk("Settings init failed (err %d)\n", err);
        }
        settings_register(&remote_conf);

        bt_conn_cb_register(&conn_callbacks);
        bt_conn_auth_info_cb_register(&auth_info_callbacks);

        err = bt_enable(bt_ready_callback);
        if (err) {
            printk("Bluetooth init failed (err %d)\n", err);
            return err;
        }

        // Register auth callbacks only if authentication is requested
        if (IS_ENABLED(CONFIG_SAMPLE_BT_USE_AUTHENTICATION)) {
            bt_conn_auth_cb_register(&auth_cb);
        }
        return 0;
    }
    // Advertising parameters for regular HID usage (Fast, Connectable, Scannable)
    static constexpr struct bt_le_adv_param adv_param_usage = {
        .id = BT_ID_DEFAULT,
        .sid = 0,
        .secondary_max_skip = 0,
        .options = kAdvOptConn | BT_LE_ADV_OPT_SCANNABLE,
        .interval_min = BT_GAP_ADV_FAST_INT_MIN_1,
        .interval_max = BT_GAP_ADV_FAST_INT_MAX_1,
        .peer = nullptr,
    };

    static int try_advertising() {
        const uint8_t current_slot = profile_mgr.active_slot();
        const uint8_t bt_id = slot_to_bt_id(current_slot);
        const std::string_view name = slot_device_name(current_slot);

        // Force stop any previous advertising set before starting a new one
        // This prevents Identity collisions (-EALREADY / -120) when switching slots.
        bt_le_adv_stop();

        // Keep GAP Device Name (0x2A00) aligned with the active slot
        bt_set_name(name.data());

        struct bt_le_adv_param param = adv_param_usage;
        param.id = bt_id;

        const std::array<struct bt_data, 3> ad_usage = {{
            { .type = BT_DATA_FLAGS, .data_len = sizeof(ad_flags), .data = ad_flags },
            { .type = BT_DATA_UUID16_ALL, .data_len = sizeof(ad_uuid), .data = ad_uuid },
            { .type = BT_DATA_NAME_COMPLETE, .data_len = static_cast<uint8_t>(name.size()), .data = reinterpret_cast<const uint8_t*>(name.data()) },
        }};

        // Select advertising data based on mode
        const struct bt_data *ad_to_use;
        size_t ad_len_to_use;
        const struct bt_data *sd_to_use = nullptr;
        size_t sd_len_to_use = 0;

        if (current_slot < 3) {
            ad_to_use = ad_usage.data();
            ad_len_to_use = ad_usage.size();
        } else {
            ad_to_use = ad_config.data();
            ad_len_to_use = ad_config.size();
            sd_to_use = sd_config.data();
            sd_len_to_use = sd_config.size();
        }

        // Print identity address for debugging
        bt_addr_le_t id_addrs[CONFIG_BT_ID_MAX];
        size_t count = CONFIG_BT_ID_MAX;
        bt_id_get(id_addrs, &count);
        char addr_str[BT_ADDR_LE_STR_LEN] = "unknown";
        if (bt_id < count) {
            bt_addr_le_to_str(&id_addrs[bt_id], addr_str, sizeof(addr_str));
        }
        printk("ADV: start slot %u (%s, BT ID %u) using addr %s\n",
               current_slot, name.data(), bt_id, addr_str);

        int err = bt_le_adv_start(&param,
                               ad_to_use, ad_len_to_use,
                               sd_to_use, sd_len_to_use);
        if (err && err != -EALREADY) {
            printk("ADV: start failed (err %d) for slot %u\n", err, current_slot);
        }
        return err;
    }

    static bool start_advertising() {
        const uint8_t current_slot = profile_mgr.active_slot();
        if (hid_is_slot_connected(current_slot)) {
            return true;
        }

        // Stop any existing advertising first
        bt_le_adv_stop();

        int err = try_advertising();
        if (!err) {
            printk("Advertising started\n");
            is_advertising = true;
            last_adv_id = current_slot;
            LedController::set_advertising(true);
            return true;
        }
        printk("Advertising start failed (err %d), watchdog will retry\n", err);
        is_advertising = false;
        LedController::set_advertising(false);
        return false;
    }

private:
    static void bt_ready_callback(int err) {
        if (err) {
            printk("Bluetooth ready failed (err %d)\n", err);
            return;
        }

        printk("Bluetooth initialized\n");

        profile_mgr.init();

        if (IS_ENABLED(CONFIG_SETTINGS)) {
            settings_load();
            printk("Settings loaded\n");
        }

        size_t id_count = 0;
        bt_id_get(nullptr, &id_count);
        while (id_count < 4) {
            int new_id = bt_id_create(nullptr, nullptr);
            if (new_id < 0) {
                printk("Failed to create BT ID (err %d)\n", new_id);
                break;
            }
            id_count++;
        }

        printk("Profile manager ready (slot %u)\n", profile_mgr.active_slot());

        const bool adv_ok = start_advertising();

        // Start the advertising watchdog
        k_work_reschedule(&adv_watchdog_work, adv_ok ? K_SECONDS(kAdvWatchdogIntervalSec) : K_MSEC(500));
        printk("Advertising watchdog started (every %ds)\n", kAdvWatchdogIntervalSec);
    }

    static void connected(struct bt_conn *conn, uint8_t err) {
        char addr[BT_ADDR_LE_STR_LEN];
        bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

        if (err) {
            printk("Connection failed: %s (0x%02x)\n", addr, err);
            is_advertising = false;
            if (!suppress_adv_restart) {
                k_work_reschedule(&adv_watchdog_work, K_MSEC(250));
            }
            return;
        }

        struct bt_conn_info info;
        uint8_t slot = profile_mgr.active_slot();
        uint8_t bt_id = slot_to_bt_id(slot);
        if (bt_conn_get_info(conn, &info) == 0) {
            bt_id = info.id;
            slot = bt_id_to_slot(bt_id);
        }

        printk("Connected (slot %u, BT ID %u): %s\n", slot, bt_id, addr);
        is_advertising = false;
        hid_set_slot_conn(slot, conn);
        LedController::set_connected(true);
        LedController::set_advertising(false);

        if (!suppress_adv_restart && !hid_is_slot_connected(profile_mgr.active_slot())) {
            k_work_reschedule(&adv_watchdog_work, K_MSEC(250));
        }

        // We stop initiating security from the nRF side.
        // Windows and Web Bluetooth will request it automatically when they access 
        // encrypted characteristics. This avoids races with the Windows HID driver.
    }

    static void disconnected(struct bt_conn *conn, uint8_t reason) {
        char addr[BT_ADDR_LE_STR_LEN];
        bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

        struct bt_conn_info info;
        uint8_t slot = profile_mgr.active_slot();
        if (bt_conn_get_info(conn, &info) == 0) {
            slot = bt_id_to_slot(info.id);
        }

        printk("Disconnected (slot %u): %s (reason 0x%02x)\n", slot, addr, reason);
        hid_clear_slot_conn(slot, conn);
        LedController::set_connected(hid_is_slot_connected(profile_mgr.active_slot()));
        // Do not call start_advertising() directly here to avoid err -12 (ENOMEM)
        // arising from executing HCI commands inside the disconnect callback context.
        // Instead, schedule the watchdog to run after a tiny delay to allow internal cleanup.
        if (!suppress_adv_restart) {
            k_work_reschedule(&adv_watchdog_work, K_MSEC(250));
        }
    }

    static void security_changed(struct bt_conn *conn, bt_security_t level, enum bt_security_err err) {
        char addr[BT_ADDR_LE_STR_LEN];
        bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

        if (!err) {
            printk("Security changed: %s level %u\n", addr, level);
        } else {
            printk("Security failed: %s level %u err %d\n", addr, level, static_cast<int>(err));
        }
    }

    static void pairing_complete(struct bt_conn *conn, bool bonded) {
        if (bonded) {
            struct bt_conn_info info;
            if (bt_conn_get_info(conn, &info) == 0) {
                const uint8_t slot = bt_id_to_slot(info.id);
                if (slot < 3) {
                    const bt_addr_le_t *addr = info.le.dst;
                    char key[32];
                    std::snprintf(key, sizeof(key), "profile/slot%u/addr", slot);
                    profile_mgr.set_slot_addr(slot, reinterpret_cast<const uint8_t*>(addr));
                    NvsStorageHal::store_addr(key, reinterpret_cast<const uint8_t*>(addr));
                    printk("REL: Bond established, identity address saved to slot %u\n", slot);
                }
            }
        }
    }

    static inline bt_conn_cb conn_callbacks = {
        .connected = connected,
        .disconnected = disconnected,
        .security_changed = security_changed,
    };

    static inline bt_conn_auth_info_cb auth_info_callbacks = {
        .pairing_complete = pairing_complete,
    };

    static inline bt_conn_auth_cb auth_cb = {
        .passkey_display = [](struct bt_conn *conn, unsigned int passkey) {
            char addr[BT_ADDR_LE_STR_LEN];
            bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
            printk("Passkey for %s: %06u\n", addr, passkey);
        },
        .cancel = [](struct bt_conn *conn) {
            char addr[BT_ADDR_LE_STR_LEN];
            bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
            printk("Pairing cancelled: %s\n", addr);
        },
    };
};

// ─── Implementation of handlers ─────────────────────────────────

static void adv_watchdog_handler(struct k_work *work)
{
    ARG_UNUSED(work);
    const uint8_t current_slot = profile_mgr.active_slot();

    // If the currently active slot is already connected, keep advertising stopped.
    if (hid_is_slot_connected(current_slot)) {
        if (is_advertising) {
            bt_le_adv_stop();
            is_advertising = false;
            LedController::set_advertising(false);
        }
        k_work_reschedule(&adv_watchdog_work, K_SECONDS(kAdvWatchdogIntervalSec));
        return;
    }

    // Force identity switch if needed (Restart if Slot changed while advertising an older slot)
    if (is_advertising && current_slot != last_adv_id) {
        printk("ADV watchdog: Profile changed (%d -> %u), restarting adv\n", last_adv_id, current_slot);
        bt_le_adv_stop();
        is_advertising = false;
    }

    // Only start/restart advertising if not already actively advertising in the desired slot
    if (!is_advertising) {
        int err = BluetoothManager::try_advertising();

        if (err == 0 || err == -EALREADY) {
            // Double-check we didn't connect asynchronously while try_advertising was blocking
            if (!hid_is_slot_connected(current_slot)) {
                is_advertising = true;
                last_adv_id = current_slot;
                LedController::set_advertising(true);
            }
        } else {
            printk("ADV watchdog: adv start err %d (slot %u)\n", err, current_slot);
            is_advertising = false;
            LedController::set_advertising(false);
            bt_le_adv_stop();
        }
    }

    k_work_reschedule(&adv_watchdog_work, K_SECONDS(kAdvWatchdogIntervalSec));
}

extern "C" void clear_bonds() {
    const uint8_t slot = profile_mgr.active_slot();
    const uint8_t bt_id = slot_to_bt_id(slot);
    printk("REL: Clearing bonds and ROTATING identity address for slot %u (BT ID %u)\n", slot, bt_id);

    suppress_adv_restart = true;
    k_work_cancel_delayable(&adv_watchdog_work);

    // 1. Stop advertising and disconnect active link for this slot before mutating identity/keys
    bt_le_adv_stop();
    is_advertising = false;
    hid_disconnect_slot(slot);

    for (int waited = 0; waited < 500 && hid_is_slot_connected(slot); waited += 20) {
        k_msleep(20);
    }

    // 2. Unpair and rotate identity in-place (HID slots 0..2 map to BT IDs 1..3)
    bt_unpair(bt_id, NULL);

    if (bt_id > 0) {
        int reset_res = bt_id_reset(bt_id, nullptr, nullptr);
        if (reset_res < 0) {
            printk("Failed to reset BT ID %u (err %d)\n", bt_id, reset_res);
        } else {
            printk("Reset BT ID %d for slot %u\n", reset_res, slot);
        }
    }
    
    // Reset our profile manager internal state
    profile_mgr.clear_active();
    
    // Visual/Audio feedback
    BuzzerController::countdown_beep();

    suppress_adv_restart = false;

    // Trigger watchdog immediately
    k_work_reschedule(&adv_watchdog_work, K_MSEC(100));
}

extern "C" void on_profile_switch_request() {
    const uint8_t old_slot = profile_mgr.active_slot();

    // Cancel pending watchdog and stop advertising before sleeping for feedback
    suppress_adv_restart = true;
    k_work_cancel_delayable(&adv_watchdog_work);
    bt_le_adv_stop();
    is_advertising = false;

    // Keep HID slots (0..2) connected across slot switches; only disconnect Slot 3 (Cam Config) on exit
    if (old_slot == 3 && hid_is_slot_connected(3)) {
        hid_disconnect_slot(3);
    }

    auto new_slot = profile_mgr.cycle();
    bt_set_name(slot_device_name(new_slot).data());
    printk("PROFILE: switched to slot %u (%s)\n", new_slot, slot_device_name(new_slot).data());
    hid_update_qdec_trigger();
    LedController::set_connected(hid_is_slot_connected(new_slot));

    // Synchronized visual + audio feedback: N+1 blinks and beeps for slot N
    for (uint8_t i = 0; i <= new_slot; ++i) {
        LedController::set_trigger_active(true);
        BuzzerController::beep();
        LedController::set_trigger_active(false);
        if (i < new_slot) k_msleep(150);
    }

    suppress_adv_restart = false;

    k_work_reschedule(&adv_watchdog_work, K_MSEC(100));
}

} // namespace remote

int main() {
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
    usb_enable(nullptr);
    k_msleep(200);
#endif
    printk("Starting Camera Remote (Modern C++)...\n");

    remote::LedController::init();
    remote::PowerController::init();
    remote::BuzzerController::init();
    remote::HidService::init();
    remote::BluetoothManager::start();
    remote::BatteryMonitor::init();
    remote::BatteryMonitor::start_periodic();

    remote::HidService::run_button_loop();

    return 0;
}
