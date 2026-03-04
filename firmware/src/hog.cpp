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

#include <array>
#include <span>
#include <cstdint>

extern "C" void led_set_trigger_active(bool active);
extern "C" void buzzer_beep();
extern "C" void buzzer_long_beep();
extern "C" void on_profile_switch_request();

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


// ─── Button Engine (Interrupt-driven) ───────────────

namespace {

const struct gpio_dt_spec btn_p5 = GPIO_DT_SPEC_GET(DT_NODELABEL(button0), gpios);
const struct gpio_dt_spec btn_p4 = GPIO_DT_SPEC_GET(DT_NODELABEL(button1), gpios);
const struct gpio_dt_spec btn_p7 = GPIO_DT_SPEC_GET(DT_NODELABEL(button2), gpios);
const struct device *const qdec_dev = DEVICE_DT_GET(DT_NODELABEL(qdec0));

// ─── Connection tracking ────────────────────────────────────────
volatile struct bt_conn *active_conn = nullptr;

// ─── Timing ─────────────────────────────────────────────────────
constexpr int kPollMs = 20;
constexpr int kDebounceMs = 60;
constexpr int kLongPressMs = 800;
constexpr int kBurstHoldMs = 2000;
constexpr int kEncoderPollMs = 50;

// ─── Button synchronization ─────────────────────────────────────
K_SEM_DEFINE(btn_sem, 0, 1);
K_SEM_DEFINE(enc_btn_sem, 0, 1);
static struct gpio_callback btn_p5_cb_data;
static struct gpio_callback btn_p4_cb_data;
static struct gpio_callback btn_p7_cb_data;

// ─── HID report (only sends if connected) ───────────────────────
static void send_hid_report(uint8_t key_bits)
{
    struct bt_conn *conn = (struct bt_conn *)active_conn;
    if (!conn) {
        return;
    }
    bt_gatt_notify(conn, &hog_svc.attrs[8], &key_bits, sizeof(key_bits));
}

// ─── Encoder Polling Timer ──────────────────────────────────────
static void encoder_timer_handler(struct k_timer *timer_id);
K_TIMER_DEFINE(encoder_timer, encoder_timer_handler, NULL);

struct ZephyrHal {
    static uint64_t get_time_ms() {
        return k_uptime_get();
    }
    static void sleep_ms(int ms) {
        k_msleep(ms);
    }
    static bool is_button_held() {
        return gpio_pin_get_dt(&btn_p5) || gpio_pin_get_dt(&btn_p4);
    }
    static bool is_encoder_button_held() {
        return gpio_pin_get_dt(&btn_p7);
    }
    static void send_hid_report(uint8_t key_bits) {
        ::remote::send_hid_report(key_bits);
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
};

using Engine = GestureEngine<ZephyrHal>;

static void encoder_timer_handler(struct k_timer *timer_id)
{
    if (!active_conn || !device_is_ready(qdec_dev)) return;

    struct sensor_value val;
    if (sensor_sample_fetch(qdec_dev) == 0 && sensor_channel_get(qdec_dev, SENSOR_CHAN_ROTATION, &val) == 0) {
        Engine::on_encoder_rotate(val.val1);
    }
}

static void button_pressed_isr(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    if (pins & BIT(btn_p7.pin)) {
        k_sem_give(&enc_btn_sem);
        return;
    }
    k_sem_give(&btn_sem);
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

// ─── Encoder button thread (long-press detection) ───────────────

K_THREAD_STACK_DEFINE(enc_btn_stack, 1024);
struct k_thread enc_btn_thread_data;

void encoder_button_thread(void *, void *, void *)
{
    printk("Encoder button thread started\n");

    while (true) {
        k_sem_take(&enc_btn_sem, K_FOREVER);
        Engine::on_encoder_button_press();
        k_sem_reset(&enc_btn_sem);
    }
}

} // namespace

// ─── Connection tracking (called from main.cpp) ─────────────────

extern "C" void hid_set_conn(struct bt_conn *conn)
{
    active_conn = bt_conn_ref(conn);
    printk("HID: conn set\n");
    k_timer_start(&encoder_timer, K_MSEC(kEncoderPollMs), K_MSEC(kEncoderPollMs));
}

extern "C" void hid_clear_conn()
{
    if (active_conn) {
        bt_conn_unref((struct bt_conn *)active_conn);
        active_conn = nullptr;
    }
    printk("HID: conn cleared\n");
    k_timer_stop(&encoder_timer);
}

// ─── Public API ──────────────────────────────────────────────────

void HidService::init()
{
    k_thread_create(&btn_thread_data, btn_stack,
                    K_THREAD_STACK_SIZEOF(btn_stack),
                    button_thread, nullptr, nullptr, nullptr,
                    7, 0, K_NO_WAIT);
    k_thread_name_set(&btn_thread_data, "btn");

    k_thread_create(&enc_btn_thread_data, enc_btn_stack,
                    K_THREAD_STACK_SIZEOF(enc_btn_stack),
                    encoder_button_thread, nullptr, nullptr, nullptr,
                    7, 0, K_NO_WAIT);
    k_thread_name_set(&enc_btn_thread_data, "enc_btn");

    if (!device_is_ready(qdec_dev)) {
        printk("QDEC sensor not ready\n");
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

void HidService::run_button_loop()
{
    gpio_pin_configure_dt(&btn_p5, GPIO_INPUT | btn_p5.dt_flags);
    gpio_pin_configure_dt(&btn_p4, GPIO_INPUT | btn_p4.dt_flags);
    gpio_pin_configure_dt(&btn_p7, GPIO_INPUT | btn_p7.dt_flags);

    gpio_pin_interrupt_configure_dt(&btn_p5, GPIO_INT_EDGE_BOTH);
    gpio_pin_interrupt_configure_dt(&btn_p4, GPIO_INT_EDGE_BOTH);
    gpio_pin_interrupt_configure_dt(&btn_p7, GPIO_INT_EDGE_BOTH);

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
