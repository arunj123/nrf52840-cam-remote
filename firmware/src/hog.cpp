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

// ─── Click Timing Engine ─────────────────────────────────────────

namespace {

const struct gpio_dt_spec btn_p5 = GPIO_DT_SPEC_GET(DT_NODELABEL(button0), gpios);
const struct gpio_dt_spec btn_p4 = GPIO_DT_SPEC_GET(DT_NODELABEL(button1), gpios);

// External C functions from main.cpp
extern "C" void led_set_trigger_active(bool active);
extern "C" void buzzer_beep();
extern "C" void buzzer_double_beep();
extern "C" void buzzer_long_beep();
extern "C" void buzzer_countdown_beep();

struct gpio_callback cb_p5;
struct gpio_callback cb_p4;

// ─── Timing constants ────────────────────────────────────────────
constexpr int64_t kDebounceMs = 80;          // Filter bounce (switches bounce ~5-20ms)
constexpr int64_t kDoubleClickWindowMs = 400; // Window for multi-click
constexpr int64_t kLongPressThresholdMs = 1000;
constexpr int64_t kCooldownMs = 300;         // Cooldown after gesture before accepting new input
constexpr int kBurstHoldMs = 2000;

// ─── State machine ──────────────────────────────────────────────
int click_count = 0;
int64_t last_press_time = 0;
int64_t last_gesture_end_time = 0;
bool gesture_in_progress = false;
bool gesture_executing = false;  // true while an action is running (blocks ISR)

struct k_work_delayable gesture_timeout_work;
struct k_work_delayable long_press_work;

// ─── HID report helpers ─────────────────────────────────────────
void send_hid_report(uint8_t key_bits)
{
    int err = bt_gatt_notify(nullptr, &hog_svc.attrs[8], &key_bits, sizeof(key_bits));
    if (err) {
        printk("HID: notify failed (err %d)\n", err);
    }
}

// ─── State reset helper ─────────────────────────────────────────
void reset_gesture_state()
{
    k_work_cancel_delayable(&gesture_timeout_work);
    k_work_cancel_delayable(&long_press_work);
    click_count = 0;
    gesture_in_progress = false;
    gesture_executing = false;
    last_gesture_end_time = k_uptime_get();
}

// ─── Gesture actions ─────────────────────────────────────────────

void action_single_click()
{
    gesture_executing = true;
    printk("GESTURE: Single Click -> Photo\n");
    led_set_trigger_active(true);

    send_hid_report(static_cast<uint8_t>(HidKey::VolumeUp));
    k_msleep(100);
    send_hid_report(0x00);

    led_set_trigger_active(false);
    reset_gesture_state();
}

void action_double_click()
{
    gesture_executing = true;
    printk("GESTURE: Double Click -> Quick double-tap Volume Up\n");
    buzzer_double_beep();
    led_set_trigger_active(true);

    // Send two quick Volume Up taps (works better than Play/Pause on camera apps)
    send_hid_report(static_cast<uint8_t>(HidKey::VolumeUp));
    k_msleep(100);
    send_hid_report(0x00);
    k_msleep(150);
    send_hid_report(static_cast<uint8_t>(HidKey::VolumeUp));
    k_msleep(100);
    send_hid_report(0x00);

    led_set_trigger_active(false);
    reset_gesture_state();
}

void action_long_press()
{
    gesture_executing = true;
    printk("GESTURE: Long Press -> Burst Mode\n");
    buzzer_long_beep();
    led_set_trigger_active(true);

    // Hold Volume Up for burst
    send_hid_report(static_cast<uint8_t>(HidKey::VolumeUp));
    k_msleep(kBurstHoldMs);
    send_hid_report(0x00);

    led_set_trigger_active(false);
    reset_gesture_state();
}

void action_self_timer()
{
    gesture_executing = true;
    printk("GESTURE: Triple Click -> Self-Timer\n");
    led_set_trigger_active(true);

    buzzer_countdown_beep();

    // Take the photo
    send_hid_report(static_cast<uint8_t>(HidKey::VolumeUp));
    k_msleep(100);
    send_hid_report(0x00);

    led_set_trigger_active(false);
    reset_gesture_state();
}

// ─── Gesture resolution ──────────────────────────────────────────

void gesture_timeout_handler(struct k_work *work)
{
    if (!gesture_in_progress) return;

    // If button is still held, don't resolve yet — let long_press_handler take over
    if (click_count == 1 && (gpio_pin_get_dt(&btn_p5) || gpio_pin_get_dt(&btn_p4))) {
        printk("GESTURE: Button still held, waiting for long-press...\n");
        return;  // long_press_work is still scheduled
    }

    // Cancel any pending long-press timer
    k_work_cancel_delayable(&long_press_work);

    int count = click_count;
    printk("GESTURE: Resolving %d click(s)\n", count);

    switch (count) {
    case 1:
        action_single_click();
        break;
    case 2:
        action_double_click();
        break;
    default:
        if (count >= 3) {
            action_self_timer();
        } else {
            reset_gesture_state();
        }
        break;
    }
}

void long_press_handler(struct k_work *work)
{
    if (!gesture_in_progress) return;

    // Check if button is still held
    if (gpio_pin_get_dt(&btn_p5) || gpio_pin_get_dt(&btn_p4)) {
        action_long_press();
    } else {
        // Button was released before 1s threshold.
        // gesture_timeout already deferred to us, so resolve now.
        printk("GESTURE: Released before long-press, resolving %d click(s)\n", click_count);
        if (click_count == 1) {
            action_single_click();
        } else {
            reset_gesture_state();
        }
    }
}

// ─── GPIO interrupt handler ──────────────────────────────────────

void button_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    // Block all input while a gesture action is executing
    if (gesture_executing) return;

    if (!(gpio_pin_get_dt(&btn_p5) || gpio_pin_get_dt(&btn_p4))) {
        return;
    }

    int64_t now = k_uptime_get();

    // Debounce: ignore bounces within 150ms of last press
    if ((now - last_press_time) < kDebounceMs) {
        return;
    }

    // Cooldown: ignore presses too soon after a gesture completed
    if ((now - last_gesture_end_time) < kCooldownMs) {
        return;
    }

    click_count++;
    last_press_time = now;

    if (click_count == 1) {
        gesture_in_progress = true;
        // Start long press timer
        k_work_reschedule(&long_press_work, K_MSEC(kLongPressThresholdMs));
    }

    // Cancel previous gesture timeout and restart it
    k_work_reschedule(&gesture_timeout_work, K_MSEC(kDoubleClickWindowMs));

    printk("GPIO: Click #%d at %lld ms\n", click_count, now);
}

} // namespace

// ─── Public HidService methods ───────────────────────────────────

void HidService::init()
{
    k_work_init_delayable(&gesture_timeout_work, gesture_timeout_handler);
    k_work_init_delayable(&long_press_work, long_press_handler);
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

    gpio_init_callback(&cb_p5, button_pressed, BIT(btn_p5.pin));
    gpio_init_callback(&cb_p4, button_pressed, BIT(btn_p4.pin));

    gpio_add_callback(btn_p5.port, &cb_p5);
    gpio_add_callback(btn_p4.port, &cb_p4);

    printk("Advanced HID Ready: Single=Photo, Double=Video, Long=Burst, Triple=SelfTimer\n");
    k_sleep(K_FOREVER);
}

} // namespace remote
