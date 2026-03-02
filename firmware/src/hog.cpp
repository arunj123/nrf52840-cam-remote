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

constexpr hids_report input_report_ref = {
    .id = 0x01,
    .type = static_cast<uint8_t>(HidReportType::Input),
};

uint8_t protocol_mode = 0x01;
uint8_t ctrl_point;

constexpr std::array<uint8_t, 43> report_map = {
    0x05, 0x0c,
    0x09, 0x01,
    0xa1, 0x01,
    0x85, 0x01,
    0x15, 0x00,
    0x25, 0x01,
    0x75, 0x01,
    0x95, 0x06,
    0x09, 0xe9,    /* Volume Increment */
    0x09, 0xea,    /* Volume Decrement */
    0x09, 0xe2,    /* Mute */
    0x09, 0xcd,    /* Play/Pause */
    0x09, 0xb5,    /* Scan Next Track */
    0x09, 0xb6,    /* Scan Previous Track */
    0x81, 0x02,
    0x95, 0x02,
    0x81, 0x03,
    0xc0
};

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

// ─── Button Handler (Thread-based state machine) ─────────────────

namespace {

const struct gpio_dt_spec btn_p5 = GPIO_DT_SPEC_GET(DT_NODELABEL(button0), gpios);
const struct gpio_dt_spec btn_p4 = GPIO_DT_SPEC_GET(DT_NODELABEL(button1), gpios);

extern "C" void led_set_trigger_active(bool active);
extern "C" void buzzer_beep();
extern "C" void buzzer_long_beep();

struct gpio_callback cb_p5;
struct gpio_callback cb_p4;

// ─── Connection tracking ────────────────────────────────────────
// Set from BLE connect/disconnect callbacks in main.cpp
volatile struct bt_conn *active_conn = nullptr;

// ─── Message queue: ISR → Thread ─────────────────────────────────
// ISR puts events here, thread reads them. Zero shared mutable state.
enum class BtnEvent : uint8_t { Press = 1 };

K_MSGQ_DEFINE(btn_msgq, sizeof(BtnEvent), 4, 1);

// ─── Timing ─────────────────────────────────────────────────────
constexpr int64_t kDebounceMs = 80;
constexpr int64_t kLongPressMs = 800;    // Threshold to detect long press
constexpr int64_t kBurstHoldMs = 2000;   // How long to hold Volume Up for burst
constexpr int64_t kPollIntervalMs = 50;  // How often to check if button is held

// ─── HID report (only sends if connected) ───────────────────────
void send_hid_report(uint8_t key_bits)
{
    struct bt_conn *conn = (struct bt_conn *)active_conn;
    if (!conn) {
        printk("HID: no connection, skipping\n");
        return;
    }
    int err = bt_gatt_notify(conn, &hog_svc.attrs[8], &key_bits, sizeof(key_bits));
    if (err) {
        printk("HID: notify failed (err %d)\n", err);
    }
}

bool is_button_held()
{
    return gpio_pin_get_dt(&btn_p5) || gpio_pin_get_dt(&btn_p4);
}

// ─── GPIO ISR: only enqueues events ─────────────────────────────
int64_t isr_last_press = 0;

void button_isr(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    if (!is_button_held()) return;  // only press, not release

    int64_t now = k_uptime_get();
    if ((now - isr_last_press) < kDebounceMs) return;
    isr_last_press = now;

    BtnEvent evt = BtnEvent::Press;
    k_msgq_put(&btn_msgq, &evt, K_NO_WAIT);
}

// ─── Button thread ──────────────────────────────────────────────
// Always beeps and flashes LED. Only sends HID if BLE is connected.

K_THREAD_STACK_DEFINE(btn_stack, 2048);
struct k_thread btn_thread_data;

void button_thread(void *, void *, void *)
{
    printk("Button thread started\n");

    while (true) {
        BtnEvent evt;
        // Block until a press event arrives
        int ret = k_msgq_get(&btn_msgq, &evt, K_FOREVER);
        if (ret != 0) continue;

        printk("BTN: Press detected\n");

        // ── STEP 1: Single click (ALWAYS beeps, sends HID if connected) ──
        led_set_trigger_active(true);
        send_hid_report(static_cast<uint8_t>(HidKey::VolumeUp));
        k_msleep(100);
        send_hid_report(0x00);
        led_set_trigger_active(false);

        // ── STEP 2: Check if button is being held for burst ──
        // Poll the button state for kLongPressMs
        bool still_held = false;
        int64_t start = k_uptime_get();

        while ((k_uptime_get() - start) < kLongPressMs) {
            k_msleep(kPollIntervalMs);
            if (is_button_held()) {
                still_held = true;
            } else {
                still_held = false;
                break;  // released, no burst
            }
        }

        if (still_held && is_button_held()) {
            // ── STEP 3: Burst mode ──
            printk("BTN: Burst mode!\n");
            buzzer_long_beep();
            led_set_trigger_active(true);

            send_hid_report(static_cast<uint8_t>(HidKey::VolumeUp));
            k_msleep(kBurstHoldMs);
            send_hid_report(0x00);

            led_set_trigger_active(false);
        }

        // ── STEP 4: Drain bounce events ──
        k_msgq_purge(&btn_msgq);

        // Small gap before accepting new input
        k_msleep(100);

        printk("BTN: Ready\n");
    }
}

} // namespace

// ─── Connection tracking (called from main.cpp BLE callbacks) ────

extern "C" void hid_set_conn(struct bt_conn *conn)
{
    active_conn = bt_conn_ref(conn);
    printk("HID: Connection set\n");
}

extern "C" void hid_clear_conn()
{
    if (active_conn) {
        bt_conn_unref((struct bt_conn *)active_conn);
        active_conn = nullptr;
    }
    printk("HID: Connection cleared\n");
}

// ─── Public HidService methods ───────────────────────────────────

void HidService::init()
{
    k_thread_create(&btn_thread_data, btn_stack,
                    K_THREAD_STACK_SIZEOF(btn_stack),
                    button_thread, nullptr, nullptr, nullptr,
                    /* priority */ 7, 0, K_NO_WAIT);
    k_thread_name_set(&btn_thread_data, "btn_thread");
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

void HidService::run_button_loop()
{
    gpio_pin_configure_dt(&btn_p5, GPIO_INPUT | btn_p5.dt_flags);
    gpio_pin_configure_dt(&btn_p4, GPIO_INPUT | btn_p4.dt_flags);

    gpio_pin_interrupt_configure_dt(&btn_p5, GPIO_INT_EDGE_TO_ACTIVE);
    gpio_pin_interrupt_configure_dt(&btn_p4, GPIO_INT_EDGE_TO_ACTIVE);

    gpio_init_callback(&cb_p5, button_isr, BIT(btn_p5.pin));
    gpio_init_callback(&cb_p4, button_isr, BIT(btn_p4.pin));

    gpio_add_callback(btn_p5.port, &cb_p5);
    gpio_add_callback(btn_p4.port, &cb_p4);

    printk("HID Ready: Press=Photo, Hold=Burst\n");
    k_sleep(K_FOREVER);
}

} // namespace remote

