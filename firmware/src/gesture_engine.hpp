#pragma once

#include "hog.hpp" // For HidKey
#include <cstdint>
#include <cstdlib>
#ifdef __ZEPHYR__
#include <zephyr/sys/printk.h>
#else
#define printk(...)
#endif

namespace remote {

/**
 * Hal concept requirements:
 * struct Hal {
 *     static uint64_t get_time_ms();
 *     static void sleep_ms(int ms);
 *     static bool is_button_held();
 *     static bool is_secondary_button_held();
 *     static bool is_encoder_button_held();
 *     static void send_hid_report(uint8_t key_bits);
 *     static void buzzer_beep();
 *     static void buzzer_long_beep();
 *     static void led_set_trigger_active(bool active);
 *     static void on_profile_switch();        // called on encoder long-press
 *     static void clear_bonds();              // called on main button 5s hold
 *     static uint8_t get_active_profile();
 *     static ConfigMappings get_config_mappings();
 * };
 */
template <typename Hal>
class GestureEngine {
public:
    static constexpr int kPollMs = 20;
    static constexpr int kDebounceMs = 60;
    static constexpr int kLongPressMs = 500;
    static constexpr int kBurstHoldMs = 2000;
    static constexpr int kResetHoldMs = 5000;
    static constexpr int kDegreesPerDetent = 60;
    static constexpr int kMaxStepsPerReport = 10;

    static void reset() {
        accumulated_degrees_ = 0;
    }

    // Called when the main button semaphore is triggered
    static void on_main_button_wake() {
        bool now = Hal::is_button_held();

        if (now) {
            // Rising edge — debounce
            Hal::sleep_ms(kDebounceMs);
            if (!Hal::is_button_held()) return;

            // Start timing
            uint64_t t0 = Hal::get_time_ms();
            bool burst = false;
            bool reset_req = false;

            // Monitor hold duration
            while (Hal::is_button_held()) {
                Hal::sleep_ms(kPollMs);
                uint64_t duration = Hal::get_time_ms() - t0;
                
                if (duration >= kResetHoldMs) { // 5 seconds reset
                    reset_req = true;
                    break;
                }
                if (!burst && duration >= kLongPressMs) {
                    burst = true;
                    // Immediate visual + audio cue at 500ms so user knows long-press registered
                    Hal::led_set_trigger_active(true);
                    Hal::buzzer_long_beep();
                    // Continue polling to check for 5s reset
                }
            }

            if (reset_req) {
                Hal::led_set_trigger_active(false);
                Hal::clear_bonds();
                while (Hal::is_button_held()) {
                    Hal::sleep_ms(kPollMs);
                }
                Hal::sleep_ms(kDebounceMs);
                return;
            }

            const ConfigMappings mappings = Hal::get_config_mappings();

            if (burst) {
                const HidKey key = mappings.main_button_long;
                if (key != HidKey::None) {
                    Hal::send_hid_report(static_cast<uint8_t>(key));
                    Hal::sleep_ms(kBurstHoldMs);
                    Hal::send_hid_report(0x00);
                }

                Hal::led_set_trigger_active(false);
                while (Hal::is_button_held()) {
                    Hal::sleep_ms(kPollMs);
                }
                Hal::sleep_ms(kDebounceMs);
                return;
            }

            // If we got here, it was a single click
            Hal::led_set_trigger_active(true);
            Hal::buzzer_beep();
            
            const HidKey key = mappings.main_button_single;
            if (key != HidKey::None) {
                Hal::send_hid_report(static_cast<uint8_t>(key));
                Hal::sleep_ms(100);
                Hal::send_hid_report(0x00);
            }
            Hal::led_set_trigger_active(false);

            Hal::sleep_ms(kDebounceMs);
        }
    }

    // Called from encoder thread when rotation is detected
    // Note: For this hardware combo, each physical detent generates 60 degrees.
    static void on_encoder_rotate(int steps) {
        accumulated_degrees_ += steps;

        const ConfigMappings mappings = Hal::get_config_mappings();
        int emitted = 0;
        while (std::abs(accumulated_degrees_) >= kDegreesPerDetent && emitted < kMaxStepsPerReport) {
            const int sign = (accumulated_degrees_ > 0) ? 1 : -1;
            accumulated_degrees_ -= sign * kDegreesPerDetent;
            ++emitted;

            // Hardware positive = Counter-Clockwise, negative = Clockwise
            const HidKey key = (sign > 0) ? mappings.encoder_ccw : mappings.encoder_cw;
            if (key != HidKey::None) {
                Hal::send_hid_report(static_cast<uint8_t>(key));
                Hal::sleep_ms(30);
                Hal::send_hid_report(0x00);
            }
        }
    }

    // Called from encoder button interrupt — detect short vs long press
    static void on_encoder_button_press() {
        // Start timing from initial wake so debounce counts toward long-press threshold
        uint64_t t0 = Hal::get_time_ms();
        Hal::sleep_ms(kDebounceMs);
        if (!Hal::is_encoder_button_held()) return;

        // Poll for long press
        while (Hal::is_encoder_button_held()) {
            Hal::sleep_ms(kPollMs);
            if ((Hal::get_time_ms() - t0) >= kLongPressMs) {
                // Long press → profile switch
                Hal::on_profile_switch();
                // Wait for release and debounce release edge
                while (Hal::is_encoder_button_held()) {
                    Hal::sleep_ms(kPollMs);
                }
                Hal::sleep_ms(kDebounceMs);
                return;
            }
        }

        // Short press
        const ConfigMappings mappings = Hal::get_config_mappings();
        const HidKey key = mappings.encoder_button_short;
        if (key != HidKey::None) {
            printk("Encoder Btn: Triggering mapped key\n");
            send_key_tap(key, 50);
        }
        Hal::sleep_ms(kDebounceMs);
    }

    // Called when the secondary button (Trigger 2 / D4) semaphore is triggered
    static void on_secondary_button_wake() {
        if (!Hal::is_secondary_button_held()) return;

        // Start timing from initial wake so debounce counts toward long-press threshold
        uint64_t t0 = Hal::get_time_ms();
        Hal::sleep_ms(kDebounceMs);
        if (!Hal::is_secondary_button_held()) return;

        while (Hal::is_secondary_button_held()) {
            Hal::sleep_ms(kPollMs);
            if ((Hal::get_time_ms() - t0) >= kLongPressMs) {
                Hal::led_set_trigger_active(true);
                Hal::buzzer_long_beep();

                const ConfigMappings mappings = Hal::get_config_mappings();
                send_key_tap(mappings.secondary_button_long, 100);
                Hal::led_set_trigger_active(false);

                while (Hal::is_secondary_button_held()) {
                    Hal::sleep_ms(kPollMs);
                }
                Hal::sleep_ms(kDebounceMs);
                return;
            }
        }

        // Released before long-press threshold: short click
        Hal::led_set_trigger_active(true);
        Hal::buzzer_beep();

        const ConfigMappings mappings = Hal::get_config_mappings();
        send_key_tap(mappings.secondary_button_single, 100);
        Hal::led_set_trigger_active(false);

        Hal::sleep_ms(kDebounceMs);
    }

private:
    static void send_key_tap(HidKey key, int duration_ms) {
        if (key != HidKey::None) {
            Hal::send_hid_report(static_cast<uint8_t>(key));
            Hal::sleep_ms(duration_ms);
            Hal::send_hid_report(0x00);
        }
    }

    static inline int accumulated_degrees_{0};
};

} // namespace remote
