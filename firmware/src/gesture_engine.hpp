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
    static constexpr int kLongPressMs = 800;
    static constexpr int kBurstHoldMs = 2000;

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
                
                if (duration >= 5000) { // 5 seconds reset
                    reset_req = true;
                    break;
                }
                if (duration >= kLongPressMs) { 
                    burst = true; 
                    // Continue polling to check for 5s reset
                }
            }

            if (reset_req) {
                Hal::clear_bonds();
                return;
            }

            uint8_t profile = Hal::get_active_profile();
            ConfigMappings mappings = Hal::get_config_mappings();

            if (burst) {
                Hal::buzzer_long_beep();
                Hal::led_set_trigger_active(true);
                
                HidKey key = (profile == 3) ? mappings.main_button_long : HidKey::VolumeUp;
                if (key != HidKey::None) {
                    Hal::send_hid_report(static_cast<uint8_t>(key));
                    Hal::sleep_ms(kBurstHoldMs);
                    Hal::send_hid_report(0x00);
                }

                Hal::led_set_trigger_active(false);
                while (Hal::is_button_held()) {
                    Hal::sleep_ms(kPollMs);
                }
                return;
            }

            // If we got here, it was a single click
            Hal::led_set_trigger_active(true);
            Hal::buzzer_beep();
            
            HidKey key = (profile == 3) ? mappings.main_button_single : HidKey::VolumeUp;
            if (key != HidKey::None) {
                Hal::send_hid_report(static_cast<uint8_t>(key));
                Hal::sleep_ms(100);
                Hal::send_hid_report(0x00);
            }
            Hal::led_set_trigger_active(false);

            Hal::sleep_ms(kDebounceMs);
        }
    }

    // Called from encoder polling timer when rotation is detected
    // Note: For this hardware combo, each physical detent generates 60 degrees.
    static void on_encoder_rotate(int steps) {
        static int accumulated_degrees = 0;
        accumulated_degrees += steps;

        if (abs(accumulated_degrees) >= 60) {
            int sign = (accumulated_degrees > 0) ? 1 : -1;
            accumulated_degrees %= 60;
            
            uint8_t profile = Hal::get_active_profile();
            ConfigMappings mappings = Hal::get_config_mappings();

            if (sign > 0) {
                // Hardware positive = Counter-Clockwise
                HidKey key = (profile == 3) ? mappings.encoder_ccw : HidKey::VolumeDown;
                if (key != HidKey::None) {
                    Hal::send_hid_report(static_cast<uint8_t>(key));
                    Hal::sleep_ms(30);
                    Hal::send_hid_report(0x00);
                }
            } else {
                // Hardware negative = Clockwise
                HidKey key = (profile == 3) ? mappings.encoder_cw : HidKey::VolumeUp;
                if (key != HidKey::None) {
                    Hal::send_hid_report(static_cast<uint8_t>(key));
                    Hal::sleep_ms(30);
                    Hal::send_hid_report(0x00);
                }
            }
        }
    }

    // Called from encoder button interrupt — detect short vs long press
    static void on_encoder_button_press() {
        // Debounce
        Hal::sleep_ms(kDebounceMs);
        if (!Hal::is_encoder_button_held()) return;

        // Poll for long press
        uint64_t t0 = Hal::get_time_ms();
        while (Hal::is_encoder_button_held()) {
            Hal::sleep_ms(kPollMs);
            if ((Hal::get_time_ms() - t0) >= kLongPressMs) {
                // Long press → profile switch
                Hal::on_profile_switch();
                // Wait for release
                while (Hal::is_encoder_button_held()) {
                    Hal::sleep_ms(kPollMs);
                }
                return;
            }
        }

        // Short press
        uint8_t profile = Hal::get_active_profile();
        ConfigMappings mappings = Hal::get_config_mappings();
        
        HidKey key = (profile == 3) ? mappings.encoder_button_short : HidKey::Mute;
        if (key != HidKey::None) {
            printk("Encoder Btn: Triggering mapped key\n");
            Hal::send_hid_report(static_cast<uint8_t>(key));
            Hal::sleep_ms(50);
            Hal::send_hid_report(0x00);
        }
    }
};

} // namespace remote
