/** @file
 *  @brief Battery Monitor Implementation
 *
 *  Uses the nRF52840's internal SAADC to measure VDD,
 *  then maps the voltage to a percentage and updates the
 *  BLE Battery Service (BAS).
 */

#include "battery.hpp"

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/bluetooth/services/bas.h>

#include <cstdint>

namespace remote {

// ─── ADC Configuration ──────────────────────────────────────────

static constexpr uint16_t kAdcResolution = 10;
static const struct device *adc_dev = DEVICE_DT_GET(DT_NODELABEL(adc));
static int16_t adc_buf;

static struct adc_channel_cfg channel_cfg = {
    .gain = ADC_GAIN_1_6,
    .reference = ADC_REF_INTERNAL,
    .acquisition_time = ADC_ACQ_TIME(ADC_ACQ_TIME_MICROSECONDS, 10),
    .channel_id = 0,
    .input_positive = SAADC_CH_PSELP_PSELP_VDD,
};

static struct adc_sequence sequence = {
    .channels = BIT(0),
    .buffer = &adc_buf,
    .buffer_size = sizeof(adc_buf),
    .resolution = kAdcResolution,
};

// ─── Voltage to Percentage mapping ──────────────────────────────

static uint16_t raw_to_millivolts(int16_t raw)
{
    if (raw < 0) raw = 0;
    return static_cast<uint16_t>((static_cast<uint32_t>(raw) * 3600) / (1 << kAdcResolution));
}

static uint8_t millivolts_to_percent(uint16_t mv)
{
    constexpr uint16_t kMinMv = 3000;
    constexpr uint16_t kMaxMv = 4200;

    if (mv <= kMinMv) return 0;
    if (mv >= kMaxMv) return 100;

    return static_cast<uint8_t>(
        (static_cast<uint32_t>(mv - kMinMv) * 100) / (kMaxMv - kMinMv)
    );
}

// ─── Periodic update timer ──────────────────────────────────────

static constexpr int kUpdateIntervalSec = 60;
static struct k_work_delayable battery_work;

static uint8_t do_read_level()
{
    if (!device_is_ready(adc_dev)) return 0;

    int err = adc_read(adc_dev, &sequence);
    if (err) {
        printk("ADC read failed (err %d)\n", err);
        return 0;
    }

    uint16_t mv = raw_to_millivolts(adc_buf);
    uint8_t percent = millivolts_to_percent(mv);

    printk("BAT: raw=%d, mV=%u, %%=%u\n", adc_buf, mv, percent);
    return percent;
}

static void battery_work_handler(struct k_work *work)
{
    uint8_t level = do_read_level();
    bt_bas_set_battery_level(level);
    printk("BAT: %u%%\n", level);

    k_work_reschedule(&battery_work, K_SECONDS(kUpdateIntervalSec));
}

// ─── BatteryMonitor methods ─────────────────────────────────────

void BatteryMonitor::init()
{
    if (!device_is_ready(adc_dev)) {
        printk("ADC device not ready\n");
        return;
    }

    int err = adc_channel_setup(adc_dev, &channel_cfg);
    if (err) {
        printk("ADC channel setup failed (err %d)\n", err);
        return;
    }

    k_work_init_delayable(&battery_work, battery_work_handler);
    printk("Battery monitor initialized\n");
}

uint8_t BatteryMonitor::read_level()
{
    return do_read_level();
}

void BatteryMonitor::start_periodic()
{
    uint8_t level = do_read_level();
    bt_bas_set_battery_level(level);

    k_work_reschedule(&battery_work, K_SECONDS(kUpdateIntervalSec));
}

} // namespace remote
