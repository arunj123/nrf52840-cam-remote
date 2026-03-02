#include "hog.hpp"
#include "battery.hpp"

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
#include <string_view>

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
        last_activity = k_uptime_get();
        k_work_init_delayable(&heartbeat_work, heartbeat_handler);
        k_work_reschedule(&heartbeat_work, K_SECONDS(kHeartbeatIntervalSec));
    }

    static void set_connected(bool connected) {
        is_connected = connected;
        wake_leds();
        apply_state();
    }

    static void set_advertising(bool advertising) {
        is_advertising = advertising;
        wake_leds();
        apply_state();
    }

    static void set_trigger_active(bool active) {
        wake_leds();
        if (active) {
            gpio_pin_set_dt(&led_green, 0);
            gpio_pin_set_dt(&led_red, 1);
            BuzzerController::beep();
        } else {
            gpio_pin_set_dt(&led_red, 0);
            apply_state();
        }
    }

    static void wake_leds() {
        last_activity = k_uptime_get();
        if (leds_dimmed) {
            leds_dimmed = false;
            apply_state();
        }
    }

private:
    static void apply_state() {
        if (leds_dimmed) return;
        gpio_pin_set_dt(&led_green, is_connected ? 1 : 0);
        gpio_pin_set_dt(&led_blue, is_advertising ? 1 : 0);
    }

    static void all_off() {
        gpio_pin_set_dt(&led_green, 0);
        gpio_pin_set_dt(&led_red, 0);
        gpio_pin_set_dt(&led_blue, 0);
    }

    // Runs in system workqueue (thread context) — safe for printk, k_msleep
    static void heartbeat_handler(struct k_work *work) {
        int64_t idle_ms = k_uptime_get() - last_activity;

        if (idle_ms > (kIdleTimeoutSec * 1000)) {
            if (!leds_dimmed) {
                all_off();
                leds_dimmed = true;
                printk("LED: Dimmed after %lld s idle\n", idle_ms / 1000);
            }

            // Quick heartbeat blink (safe — we're in thread context)
            if (is_connected) {
                gpio_pin_set_dt(&led_green, 1);
                k_msleep(50);
                gpio_pin_set_dt(&led_green, 0);
            } else if (is_advertising) {
                gpio_pin_set_dt(&led_blue, 1);
                k_msleep(50);
                gpio_pin_set_dt(&led_blue, 0);
            }
        }

        // Reschedule ourselves
        k_work_reschedule(&heartbeat_work, K_SECONDS(kHeartbeatIntervalSec));
    }

    static inline const struct gpio_dt_spec led_green = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
    static inline const struct gpio_dt_spec led_red = GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios);
    static inline const struct gpio_dt_spec led_blue = GPIO_DT_SPEC_GET(DT_ALIAS(led2), gpios);

    static inline struct k_work_delayable heartbeat_work;
    static inline int64_t last_activity = 0;
    static inline bool is_connected = false;
    static inline bool is_advertising = false;
    static inline bool leds_dimmed = false;

    static constexpr int kIdleTimeoutSec = 30;
    static constexpr int kHeartbeatIntervalSec = 5;
};

} // namespace remote


extern "C" void led_set_trigger_active(bool active) {
    remote::LedController::set_trigger_active(active);
}

extern "C" void led_wake() {
    remote::LedController::wake_leds();
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

class BluetoothManager {
public:
    static int start() {
        int err = bt_enable(bt_ready_callback);
        if (err) {
            printk("Bluetooth init failed (err %d)\n", err);
            return err;
        }

        bt_conn_auth_cb_register(&auth_cb);
        return 0;
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

        start_advertising();
    }

    static void start_advertising() {
        static constexpr uint8_t ad_flags[] = { BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR };
        static constexpr uint8_t ad_uuid[] = { 
            BT_UUID_16_ENCODE(BT_UUID_HIDS_VAL),
            BT_UUID_16_ENCODE(BT_UUID_BAS_VAL) 
        };
        static constexpr std::string_view name = CONFIG_BT_DEVICE_NAME;

        static const std::array<struct bt_data, 3> ad = {{
            { .type = BT_DATA_FLAGS, .data_len = sizeof(ad_flags), .data = ad_flags },
            { .type = BT_DATA_UUID16_ALL, .data_len = sizeof(ad_uuid), .data = ad_uuid },
            { .type = BT_DATA_NAME_COMPLETE, .data_len = static_cast<uint8_t>(name.size()), .data = reinterpret_cast<const uint8_t*>(name.data()) },
        }};

        static constexpr uint8_t sd_uuid[] = {
            0xf0, 0xde, 0xbc, 0x9a, 0x78, 0x56, 0x34, 0x12,
            0x78, 0x56, 0x34, 0x12, 0x78, 0x56, 0x34, 0x12
        };

        static const std::array<struct bt_data, 1> sd = {{
            { .type = BT_DATA_UUID128_ALL, .data_len = sizeof(sd_uuid), .data = sd_uuid },
        }};

        int err = bt_le_adv_start(BT_LE_ADV_CONN_FAST_1, ad.data(), ad.size(), sd.data(), sd.size());
        if (err) {
            printk("Advertising failed (err %d)\n", err);
        } else {
            printk("Advertising started\n");
            LedController::set_advertising(true);
        }
    }

    static void connected(struct bt_conn *conn, uint8_t err) {
        char addr[BT_ADDR_LE_STR_LEN];
        bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

        if (err) {
            printk("Connection failed: %s (0x%02x)\n", addr, err);
            return;
        }

        printk("Connected: %s\n", addr);
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
        hid_clear_conn();
        LedController::set_connected(false);
        start_advertising();
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

    static inline bt_conn_cb conn_callbacks = {
        .connected = connected,
        .disconnected = disconnected,
        .security_changed = security_changed,
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
        ConnCbReg() { bt_conn_cb_register(&conn_callbacks); }
    };
    static inline ConnCbReg cb_reg;
};

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
