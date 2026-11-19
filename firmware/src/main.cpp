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
extern "C" void hid_set_conn(struct bt_conn *conn);
extern "C" void hid_clear_conn();
extern "C" void hid_disconnect();



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
        ConfigMappings mappings;
        if (read_cb(cb_arg, &mappings, sizeof(mappings)) > 0) {
            HidService::set_config_mappings(mappings);
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
static volatile bool is_connected = false;
static volatile bool is_advertising = false;
static int adv_tick_count = 0;       // Count watchdog ticks to toggle modes
static bool use_directed_adv = true; // Toggle between directed and undirected
static int8_t last_adv_id = -1;      // Track which identity was last used for advertising
static bool last_adv_mode_directed = false;

// ─── Advertising data (shared between start and watchdog) ───────
static constexpr uint8_t ad_flags[] = { BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR };
static constexpr uint8_t ad_uuid[] = {
    BT_UUID_16_ENCODE(BT_UUID_HIDS_VAL),
    BT_UUID_16_ENCODE(BT_UUID_BAS_VAL)
};
static constexpr std::string_view dev_name = CONFIG_BT_DEVICE_NAME;

// Advertising: HID Mode (Profiles 0-2) — Advertises HIDS and BAS
static const std::array<struct bt_data, 3> ad_usage = {{
    { .type = BT_DATA_FLAGS, .data_len = sizeof(ad_flags), .data = ad_flags },
    { .type = BT_DATA_UUID16_ALL, .data_len = sizeof(ad_uuid), .data = ad_uuid },
    { .type = BT_DATA_NAME_COMPLETE, .data_len = static_cast<uint8_t>(dev_name.size()), .data = reinterpret_cast<const uint8_t*>(dev_name.data()) },
}};

static constexpr uint8_t sd_uuid[] = {
    0x2b, 0x7a, 0x00, 0x86, 0xf0, 0xa8, 0xe5, 0xa3,
    0xaa, 0x4d, 0xd9, 0xb7, 0xa5, 0x71, 0x45, 0x9e
};

// Advertising: Config Mode (Profile 3) — Advertises Custom Service and BAS
static constexpr std::string_view config_name = "Cam Config";
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

    static int try_advertising(bool force_undirected = false) {
        auto addr_opt = profile_mgr.active_addr();
        bool config_mode = profile_mgr.active_slot() == 3;
        uint8_t current_slot = profile_mgr.active_slot();
        if (addr_opt && !force_undirected && !config_mode) {
            bt_addr_le_t target;
            std::memcpy(&target, addr_opt->data(), 7);
            
            char addr_str[BT_ADDR_LE_STR_LEN];
            bt_addr_le_to_str(&target, addr_str, sizeof(addr_str));
            // Only print once per mode change to avoid log spam
            if (adv_tick_count == 0 || !use_directed_adv) {
                printk("ADV: Directed advertising to %s\n", addr_str);
            }
            
            struct bt_le_adv_param param = {
                .id = profile_mgr.active_slot(),
                .sid = 0,
                .secondary_max_skip = 0,
                .options = kAdvOptConn | BT_LE_ADV_OPT_DIR_MODE_LOW_DUTY | BT_LE_ADV_OPT_DIR_ADDR_RPA,
                .interval_min = BT_GAP_ADV_FAST_INT_MIN_1,
                .interval_max = BT_GAP_ADV_FAST_INT_MAX_1,
                .peer = &target,
            };
            
            return bt_le_adv_start(&param, nullptr, 0, nullptr, 0);
        } else {
            // Force stop any previous advertising set before starting a new one
            // This prevents Identity collisions (-EALREADY / -120) when switching slots.
            bt_le_adv_stop();

            struct bt_le_adv_param param = adv_param_usage;
            param.id = current_slot;
            
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
            if (current_slot < count) {
                bt_addr_le_to_str(&id_addrs[current_slot], addr_str, sizeof(addr_str));
            }
            printk("ADV: start slot %d using ID addr %s\n", current_slot, addr_str);

            int err = bt_le_adv_start(&param,
                                   ad_to_use, ad_len_to_use,
                                   sd_to_use, sd_len_to_use);
            if (err && err != -EALREADY) {
                printk("ADV: start failed (err %d) for slot %u\n", err, current_slot);
            }
            return err;
        }
    }

    static bool start_advertising() {
        // Stop any existing advertising first
        bt_le_adv_stop();

        int err = try_advertising();
        if (!err) {
            const uint8_t current_slot = profile_mgr.active_slot();
            printk("Advertising started\n");
            is_advertising = true;
            last_adv_id = current_slot;
            last_adv_mode_directed = (current_slot < 3) && profile_mgr.active_addr().has_value() && use_directed_adv;
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
        HidService::init();

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
            is_connected = false;
            is_advertising = false;
            // The watchdog will restart advertising safely
            k_work_reschedule(&adv_watchdog_work, K_MSEC(250));
            return;
        }

        printk("Connected: %s\n", addr);
        is_connected = true;
        is_advertising = false;
        hid_set_conn(conn);
        LedController::set_connected(true);
        LedController::set_advertising(false);

        // We stop initiating security from the nRF side.
        // Windows and Web Bluetooth will request it automatically when they access 
        // encrypted characteristics. This avoids races with the Windows HID driver.
    }

    static void disconnected(struct bt_conn *conn, uint8_t reason) {
        char addr[BT_ADDR_LE_STR_LEN];
        bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

        printk("Disconnected: %s (reason 0x%02x)\n", addr, reason);
        is_connected = false;
        hid_clear_conn();
        LedController::set_connected(false);
        // Do not call start_advertising() directly here to avoid err -12 (ENOMEM)
        // arising from executing HCI commands inside the disconnect callback context.
        // Instead, schedule the watchdog to run after a tiny delay to allow internal cleanup.
        k_work_reschedule(&adv_watchdog_work, K_MSEC(250));
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
            if (bt_conn_get_info(conn, &info) == 0 && profile_mgr.active_slot() < 3) {
                const bt_addr_le_t *addr = info.le.dst;
                profile_mgr.update_active_addr(reinterpret_cast<const uint8_t*>(addr));
                printk("REL: Bond established, identity address saved to slot %u\n", profile_mgr.active_slot());
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
    // If connected, no need to advertise — just reschedule
    if (is_connected) {
        adv_tick_count = 0;
        k_work_reschedule(&adv_watchdog_work, K_SECONDS(kAdvWatchdogIntervalSec));
        return;
    }

    uint8_t current_id = profile_mgr.active_slot();
    const bool has_bond = (current_id < 3) && profile_mgr.active_addr().has_value();
    bool should_be_directed = has_bond && use_directed_adv;

    // Toggle logic: if we have a bond, swap between directed and undirected every 20 seconds (4 ticks)
    if (has_bond) {
        adv_tick_count++;
        if (adv_tick_count > 4) {
            adv_tick_count = 0;
            use_directed_adv = !use_directed_adv;
            should_be_directed = use_directed_adv;
            printk("ADV watchdog: Toggle mode to %s\n", should_be_directed ? "directed" : "undirected");
            bt_le_adv_stop(); // Force restart in new mode
            is_advertising = false;
        }
    } else {
        adv_tick_count = 0;
    }

    // Force identity switch if needed (Restart if Slot or Mode changed)
    if (is_advertising && (current_id != last_adv_id || should_be_directed != last_adv_mode_directed)) {
        printk("ADV watchdog: Profile/Mode changed (%d -> %d), restarting adv\n", last_adv_id, current_id);
        bt_le_adv_stop();
        is_advertising = false;
    }

    // Only start/restart advertising if not already actively advertising in the desired slot/mode
    if (!is_advertising) {
        int err = BluetoothManager::try_advertising(!should_be_directed);

        if (err == 0 || err == -EALREADY) {
            // Double-check we didn't connect asynchronously while try_advertising was blocking
            if (!is_connected) {
                is_advertising = true;
                last_adv_id = current_id;
                last_adv_mode_directed = should_be_directed;
                LedController::set_advertising(true);
            }
        } else {
            printk("ADV watchdog: adv start err %d (slot %u)\n", err, current_id);
            is_advertising = false;
            LedController::set_advertising(false);
            bt_le_adv_stop();
        }
    }

    k_work_reschedule(&adv_watchdog_work, K_SECONDS(kAdvWatchdogIntervalSec));
}

extern "C" void clear_bonds() {
    uint8_t id = profile_mgr.active_slot();
    printk("REL: Clearing bonds and ROTATING identity address for slot %u\n", id);
    
    // 1. Unpair
    bt_unpair(id, NULL);

    // 2. Stop everything and disconnect
    bt_le_adv_stop();
    is_advertising = false;
    hid_disconnect();
    
    // 3. Reset identity in-place (IDs > 0) to change the random address
    // This forces Windows to see it as a brand new device without losing the slot.
    if (id > 0) {
        int reset_res = bt_id_reset(id, nullptr, nullptr);
        if (reset_res < 0) {
            printk("Failed to reset BT ID %u (err %d)\n", id, reset_res);
        } else {
            printk("Reset BT ID %d for slot %u\n", reset_res, id);
        }
    }
    
    // Reset our profile manager internal state
    profile_mgr.clear_active();
    
    // Visual/Audio feedback
    BuzzerController::countdown_beep();
    
    adv_tick_count = 0;
    use_directed_adv = false; // Start with undirected to be seen
    
    // Trigger watchdog immediately
    k_work_reschedule(&adv_watchdog_work, K_MSEC(100));
}

extern "C" void on_profile_switch_request() {
    auto new_slot = profile_mgr.cycle();
    printk("PROFILE: switched to slot %u\n", new_slot);

    // Buzzer feedback: beep N+1 times for slot N
    for (uint8_t i = 0; i <= new_slot; ++i) {
        BuzzerController::beep();
        if (i < new_slot) k_msleep(150);
    }

    // Explicitly stop advertising to ensure the identity change is applied
    bt_le_adv_stop();
    is_advertising = false;

    // Disconnect current connection so we can re-advertise to the new slot's peer
    hid_disconnect();
    
    // Reset watchdog ticks so we start fresh with the new profile
    adv_tick_count = 0;
    use_directed_adv = true; 

    // Trigger watchdog immediately if not connected, otherwise wait for disconnect callback
    if (!is_connected) {
        k_work_reschedule(&adv_watchdog_work, K_MSEC(100));
    }
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
    remote::BluetoothManager::start();
    remote::BatteryMonitor::init();
    remote::BatteryMonitor::start_periodic();

    remote::HidService::run_button_loop();

    return 0;
}
