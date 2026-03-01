/** @file
 *  @brief HoG Service Implementation (Modern C++20)
 */

#include "hog.hpp"

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

#include <array>
#include <span>
#include <cstdint>

namespace remote {

namespace {

enum class HidFlags : uint8_t {
    RemoteWake = BIT(0),
    NormallyConnectable = BIT(1),
};

struct hids_info {
    uint16_t version; /* version number of base USB HID Specification */
    uint8_t code;    /* country HID Device hardware is localized for. */
    uint8_t flags;
} __packed;

struct hids_report {
    uint8_t id;   /* report id */
    uint8_t type; /* report type */
} __packed;

constexpr hids_info info = {
    .version = 0x0111, /* HID 1.11 */
    .code = 0x00,
    .flags = static_cast<uint8_t>(HidFlags::NormallyConnectable),
};

enum class HidReportType : uint8_t {
    Input = 0x01,
    Output = 0x02,
    Feature = 0x03,
};

constexpr hids_report input_report_ref = {
    .id = 0x01,
    .type = static_cast<uint8_t>(HidReportType::Input),
};

uint8_t protocol_mode = 0x01; /* Report Protocol Mode */
uint8_t ctrl_point;

constexpr std::array<uint8_t, 43> report_map = {
    0x05, 0x0c,                    /* Usage Page (Consumer Devices) */
    0x09, 0x01,                    /* Usage (Consumer Control) */
    0xa1, 0x01,                    /* Collection (Application) */
    0x85, 0x01,                    /*   Report ID (1) */
    0x15, 0x00,                    /*   Logical Minimum (0) */
    0x25, 0x01,                    /*   Logical Maximum (1) */
    0x75, 0x01,                    /*   Report Size (1) */
    0x95, 0x06,                    /*   Report Count (6) */
    0x09, 0xe9,                    /*   Usage (Volume Increment) */
    0x09, 0xea,                    /*   Usage (Volume Decrement) */
    0x09, 0xe2,                    /*   Usage (Mute) */
    0x09, 0xcd,                    /*   Usage (Play/Pause) */
    0x09, 0xb5,                    /*   Usage (Scan Next Track) */
    0x09, 0xb6,                    /*   Usage (Scan Previous Track) */
    0x81, 0x02,                    /*   Input (Data,Variable,Absolute) */
    0x95, 0x02,                    /*   Report Count (2) padding */
    0x81, 0x03,                    /*   Input (Constant,Variable,Absolute) */
    0xc0                           /* End Collection */
};

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
    static constexpr uint8_t zero_report = 0;
    return bt_gatt_attr_read(conn, attr, buf, len, offset, &zero_report, sizeof(zero_report));
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

ssize_t read_protocol_mode(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                           void *buf, uint16_t len, uint16_t offset)
{
    return bt_gatt_attr_read(conn, attr, buf, len, offset, &protocol_mode,
                             sizeof(protocol_mode));
}

ssize_t write_protocol_mode(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                            const void *buf, uint16_t len, uint16_t offset, uint8_t flags)
{
    if (offset + len > sizeof(protocol_mode)) {
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
    }

    memcpy(&protocol_mode + offset, buf, len);
    return len;
}

} // namespace

/* HID Service Declaration */
BT_GATT_SERVICE_DEFINE(hog_svc,
    BT_GATT_PRIMARY_SERVICE(BT_UUID_HIDS),
    BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_PROTOCOL_MODE,
                           BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE_WITHOUT_RESP,
                           BT_GATT_PERM_READ | BT_GATT_PERM_WRITE,
                           read_protocol_mode, write_protocol_mode,
                           &protocol_mode),
    BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_INFO, BT_GATT_CHRC_READ,
                           BT_GATT_PERM_READ, read_info, nullptr, const_cast<hids_info*>(&info)),
    BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_REPORT_MAP, BT_GATT_CHRC_READ,
                           BT_GATT_PERM_READ, read_report_map, nullptr, nullptr),
    BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_REPORT,
                           BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
                           SAMPLE_BT_PERM_READ,
                           read_input_report, nullptr, nullptr),
    BT_GATT_CCC(nullptr, SAMPLE_BT_PERM_READ | SAMPLE_BT_PERM_WRITE),
    BT_GATT_DESCRIPTOR(BT_UUID_HIDS_REPORT_REF, BT_GATT_PERM_READ,
                       read_report_ref, nullptr, const_cast<hids_report*>(&input_report_ref)),
    BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_CTRL_POINT,
                           BT_GATT_CHRC_WRITE_WITHOUT_RESP,
                           BT_GATT_PERM_WRITE,
                           nullptr, write_ctrl_point, &ctrl_point),
);

namespace {

const struct gpio_dt_spec btn_p5 = GPIO_DT_SPEC_GET(DT_NODELABEL(button0), gpios);
const struct gpio_dt_spec btn_p4 = GPIO_DT_SPEC_GET(DT_NODELABEL(button1), gpios);

extern "C" void led_set_trigger_active(bool active);

struct gpio_callback cb_p5;
struct gpio_callback cb_p4;

struct k_work button_work;
bool button_is_pressed;

void button_work_handler(struct k_work *work)
{
    if (!button_is_pressed) return;

    led_set_trigger_active(true);

    // Send Volume Up (Bit 0 in the 1-byte report)
    uint8_t report_data = 0x01;
    std::span<uint8_t> report{&report_data, 1};

    int err = bt_gatt_notify(nullptr, &hog_svc.attrs[8], report.data(), report.size());
    if (err) {
        printk("HID: Vol UP notify failed (err %d)\n", err);
    } else {
        printk("HID: Sent Vol UP\n");
    }

    k_msleep(100);

    report_data = 0x00; // Released
    err = bt_gatt_notify(nullptr, &hog_svc.attrs[8], report.data(), report.size());
    if (err) {
        printk("HID: Release notify failed (err %d)\n", err);
    } else {
        printk("HID: Sent Release\n");
    }

    led_set_trigger_active(false);
    k_msleep(200);
    button_is_pressed = false;
}

void button_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    if (button_is_pressed) return;

    if (gpio_pin_get_dt(&btn_p5) || gpio_pin_get_dt(&btn_p4)) {
        button_is_pressed = true;
        k_work_submit(&button_work);
        printk("GPIO: Trigger detected\n");
    }
}

} // namespace

void HidService::init()
{
    k_work_init(&button_work, button_work_handler);
}

void HidService::run_button_loop()
{
    gpio_pin_configure_dt(&btn_p5, GPIO_INPUT | btn_p5.dt_flags);
    gpio_pin_configure_dt(&btn_p4, GPIO_INPUT | btn_p4.dt_flags);

    gpio_pin_interrupt_configure_dt(&btn_p5, GPIO_INT_EDGE_TO_ACTIVE);
    gpio_pin_interrupt_configure_dt(&btn_p4, GPIO_INT_EDGE_TO_ACTIVE);

    gpio_init_callback(&cb_p5, button_pressed, BIT(btn_p5.pin));
    gpio_init_callback(&cb_p4, button_pressed, BIT(btn_p4.pin));

    gpio_add_callback(btn_p5.port, &cb_p5);
    gpio_add_callback(btn_p4.port, &cb_p4);

    printk("Single-trigger HID Ready.\n");
    k_sleep(K_FOREVER);
}

} // namespace remote
