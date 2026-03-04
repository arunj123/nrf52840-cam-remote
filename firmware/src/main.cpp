#include "hog.hpp"
#include "battery.hpp"
#include "profile_manager.hpp"
#include "provisioning_service.hpp"

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

#include <array>
#include <optional>
#include <string_view>
#include <cstring>

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
        gpio_pin_configure_dt(&led_green, GPIO_OUTPUT_INACTIVE);
        gpio_pin_configure_dt(&led_red, GPIO_OUTPUT_INACTIVE);
        gpio_pin_configure_dt(&led_blue, GPIO_OUTPUT_INACTIVE);
    }

    static void set_connected(bool connected) {
        gpio_pin_set_dt(&led_green, 0); // Disable LED to save power during idle sleep
    }

    static void set_advertising(bool advertising) {
        gpio_pin_set_dt(&led_blue, 0); // Disable LED to save power during idle sleep
    }

    static void set_trigger_active(bool active) {
        if (active) {
            gpio_pin_set_dt(&led_green, 0);
            gpio_pin_set_dt(&led_red, 1);
            BuzzerController::beep();
        } else {
            gpio_pin_set_dt(&led_red, 0);
            gpio_pin_set_dt(&led_green, 0); // Keep green OFF to maintain dark sleep state
        }
    }

private:
    static inline const struct gpio_dt_spec led_green = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
    static inline const struct gpio_dt_spec led_red = GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios);
    static inline const struct gpio_dt_spec led_blue = GPIO_DT_SPEC_GET(DT_ALIAS(led2), gpios);
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
        char key_str[64];
        std::snprintf(key_str, sizeof(key_str), "remote/%.*s", static_cast<int>(key.size()), key.data());
        return std::nullopt; // Handled by settings handler
    }

    static bool store_addr(std::string_view key, const uint8_t* addr) {
        char key_str[64];
        std::snprintf(key_str, sizeof(key_str), "remote/%.*s", static_cast<int>(key.size()), key.data());
        int err = settings_save_one(key_str, addr, 7);
        return err == 0;
    }

    static std::optional<std::array<uint8_t, 7>> load_addr(std::string_view key) {
        return std::nullopt; // Will be handled by settings_load() and a handler
    }
};

static ProfileManager<NvsStorageHal, 3> profile_mgr;

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
    for (uint8_t i = 0; i < 3; ++i) {
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

// ─── Advertising data (shared between start and watchdog) ───────
static constexpr uint8_t ad_flags[] = { BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR };
static constexpr uint8_t ad_uuid[] = {
    BT_UUID_16_ENCODE(BT_UUID_HIDS_VAL),
    BT_UUID_16_ENCODE(BT_UUID_BAS_VAL)
};
static constexpr std::string_view dev_name = CONFIG_BT_DEVICE_NAME;

static const std::array<struct bt_data, 3> ad = {{
    { .type = BT_DATA_FLAGS, .data_len = sizeof(ad_flags), .data = ad_flags },
    { .type = BT_DATA_UUID16_ALL, .data_len = sizeof(ad_uuid), .data = ad_uuid },
    { .type = BT_DATA_NAME_COMPLETE, .data_len = static_cast<uint8_t>(dev_name.size()), .data = reinterpret_cast<const uint8_t*>(dev_name.data()) },
}};

static constexpr uint8_t sd_uuid[] = {
    0xf0, 0xde, 0xbc, 0x9a, 0x78, 0x56, 0x34, 0x12,
    0x78, 0x56, 0x34, 0x12, 0x78, 0x56, 0x34, 0x12
};

static const std::array<struct bt_data, 1> sd = {{
    { .type = BT_DATA_UUID128_ALL, .data_len = sizeof(sd_uuid), .data = sd_uuid },
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

        err = bt_enable(bt_ready_callback);
        if (err) {
            printk("Bluetooth init failed (err %d)\n", err);
            return err;
        }

        bt_conn_auth_cb_register(&auth_cb);
        return 0;
    }

    static int try_advertising() {
        auto addr_opt = profile_mgr.active_addr();
        
        if (addr_opt) {
            bt_addr_le_t target;
            std::memcpy(&target, addr_opt->data(), 7);
            
            char addr_str[BT_ADDR_LE_STR_LEN];
            bt_addr_le_to_str(&target, addr_str, sizeof(addr_str));
            printk("ADV: Directed advertising to %s\n", addr_str);
            
            struct bt_le_adv_param param = {
                .id = BT_ID_DEFAULT,
                .sid = 0,
                .secondary_max_skip = 0,
                .options = BT_LE_ADV_OPT_CONN,
                .interval_min = BT_GAP_ADV_FAST_INT_MIN_1,
                .interval_max = BT_GAP_ADV_FAST_INT_MAX_1,
                .peer = &target,
            };
            
            return bt_le_adv_start(&param, nullptr, 0, nullptr, 0);
        } else {
            return bt_le_adv_start(BT_LE_ADV_CONN_FAST_1,
                                   ad.data(), ad.size(),
                                   sd.data(), sd.size());
        }
    }

    static void start_advertising() {
        // Stop any existing advertising first
        bt_le_adv_stop();

        // Retry advertising up to 3 times with fast intervals
        for (int attempt = 0; attempt < 3; ++attempt) {
            int err = try_advertising();
            if (!err) {
                printk("Advertising started (attempt %d)\n", attempt + 1);
                is_advertising = true;
                LedController::set_advertising(true);
                return;
            }
            printk("Advertising failed (err %d), retry %d\n", err, attempt + 1);
            k_msleep(500);
            bt_le_adv_stop();
        }
        printk("Advertising FAILED after 3 attempts!\n");
        is_advertising = false;
        LedController::set_advertising(false);
    }

private:
    static void bt_ready_callback(int err) {
        if (err) {
            printk("Bluetooth ready failed (err %d)\n", err);
            return;
        }

        printk("Bluetooth initialized\n");
        HidService::init();

        if (IS_ENABLED(CONFIG_SETTINGS)) {
            settings_load();
            printk("Settings loaded\n");
        }

        profile_mgr.init();
        printk("Profile manager init (slot %u)\n", profile_mgr.active_slot());

        start_advertising();

        // Start the advertising watchdog
        k_work_reschedule(&adv_watchdog_work, K_SECONDS(kAdvWatchdogIntervalSec));
        printk("Advertising watchdog started (every %ds)\n", kAdvWatchdogIntervalSec);
    }

    static void connected(struct bt_conn *conn, uint8_t err) {
        char addr[BT_ADDR_LE_STR_LEN];
        bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

        if (err) {
            printk("Connection failed: %s (0x%02x)\n", addr, err);
            is_connected = false;
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

        if (int sec_err = bt_conn_set_security(conn, BT_SECURITY_L2); sec_err < 0) {
            printk("Failed to set security (err %d)\n", sec_err);
        }
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
            const bt_addr_le_t *addr = bt_conn_get_dst(conn);
            profile_mgr.update_active_addr(reinterpret_cast<const uint8_t*>(addr));
            printk("REL: Bond established, address saved to slot %u\n", profile_mgr.active_slot());
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
        .cancel = [](struct bt_conn *conn) {
            char addr[BT_ADDR_LE_STR_LEN];
            bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
            printk("Pairing cancelled: %s\n", addr);
        },
    };

    // Registration helper
    struct ConnCbReg {
        ConnCbReg() { 
            bt_conn_cb_register(&conn_callbacks);
            bt_conn_auth_info_cb_register(&auth_info_callbacks);
        }
    };
    static inline ConnCbReg cb_reg;
};

// ─── Implementation of handlers ─────────────────────────────────

static void adv_watchdog_handler(struct k_work *work)
{
    // If connected, no need to advertise — just reschedule
    if (is_connected) {
        k_work_reschedule(&adv_watchdog_work, K_SECONDS(kAdvWatchdogIntervalSec));
        return;
    }

    // Try to start advertising (will return -EALREADY if already running)
    int err = BluetoothManager::try_advertising();

    if (err == 0) {
        // Advertising was NOT running — we just restarted it
        printk("ADV watchdog: re-started advertising\n");
        is_advertising = true;
        LedController::set_advertising(true);
    } else if (err == -EALREADY) {
        // Advertising is already running — all good
        if (!is_advertising) {
            // Fix stale state
            is_advertising = true;
            LedController::set_advertising(true);
        }
    } else {
        // Real error — advertising failed
        printk("ADV watchdog: adv start err %d\n", err);
        is_advertising = false;
        LedController::set_advertising(false);
        // Try stopping cleanly so next attempt has a fresh start
        bt_le_adv_stop();
    }

    k_work_reschedule(&adv_watchdog_work, K_SECONDS(kAdvWatchdogIntervalSec));
}

extern "C" void on_profile_switch_request() {
    auto new_slot = profile_mgr.cycle();
    printk("PROFILE: switched to slot %u\n", new_slot);

    // Buzzer feedback: beep N+1 times for slot N
    for (uint8_t i = 0; i <= new_slot; ++i) {
        BuzzerController::beep();
        if (i < new_slot) k_msleep(150);
    }

    // Disconnect current connection so we can re-advertise to the new slot's peer
    hid_clear_conn();
    
    // Force immediate re-advertising
    BluetoothManager::start_advertising();
}

} // namespace remote

int main() {
    printk("Starting Camera Remote (Modern C++)...\n");

    remote::LedController::init();
    remote::BuzzerController::init();
    remote::BluetoothManager::start();
    remote::BatteryMonitor::init();
    remote::BatteryMonitor::start_periodic();

    remote::HidService::run_button_loop();

    return 0;
}
