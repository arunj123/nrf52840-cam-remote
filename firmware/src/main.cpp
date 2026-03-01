#include "hog.hpp"

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

#include <array>
#include <string_view>

namespace remote {

class BuzzerController {
public:
    static void init() {
        if (device_is_ready(buzzer.port)) {
            gpio_pin_configure_dt(&buzzer, GPIO_OUTPUT_INACTIVE);
        }
    }

    static void beep() {
        if (device_is_ready(buzzer.port)) {
            gpio_pin_set_dt(&buzzer, 1);
            k_sleep(K_MSEC(50));
            gpio_pin_set_dt(&buzzer, 0);
        }
    }

private:
    static inline const struct gpio_dt_spec buzzer = GPIO_DT_SPEC_GET(DT_ALIAS(buzzer), gpios);
};

class LedController {
public:
    static void init() {
        gpio_pin_configure_dt(&led_green, GPIO_OUTPUT_INACTIVE);
        gpio_pin_configure_dt(&led_red, GPIO_OUTPUT_INACTIVE);
        gpio_pin_configure_dt(&led_blue, GPIO_OUTPUT_INACTIVE);
    }

    static void set_connected(bool connected) {
        gpio_pin_set_dt(&led_green, connected ? 1 : 0);
    }

    static void set_advertising(bool advertising) {
        gpio_pin_set_dt(&led_blue, advertising ? 1 : 0);
    }

    static void set_trigger_active(bool active) {
        if (active) {
            gpio_pin_set_dt(&led_green, 0);
            gpio_pin_set_dt(&led_red, 1);
            BuzzerController::beep();
        } else {
            gpio_pin_set_dt(&led_red, 0);
            gpio_pin_set_dt(&led_green, 1);
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

    remote::HidService::run_button_loop();

    return 0;
}
