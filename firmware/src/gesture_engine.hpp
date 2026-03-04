#pragma once

#include "hog.hpp" // For HidKey
#include <cstdint>

namespace remote {

/**
 * Hal concept requirements:
 * struct Hal {
 *     static uint64_t get_time_ms();
 *     static void sleep_ms(int ms);
 *     static bool is_button_held();
 *     static bool is_encoder_button_held();
 *     static void send_hid_report(uint8_t key_bits);
 *     static void buzzer_long_beep();
 *     static void led_set_trigger_active(bool active);
 *     static void on_profile_switch();        // called on encoder long-press
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

            // Single click
            Hal::led_set_trigger_active(true);
            Hal::send_hid_report(static_cast<uint8_t>(HidKey::VolumeUp));
            Hal::sleep_ms(100);
            Hal::send_hid_report(0x00);
            Hal::led_set_trigger_active(false);

            // Check for long press (poll while held)
            uint64_t t0 = Hal::get_time_ms();
            bool burst = false;
            while (Hal::is_button_held()) {
                Hal::sleep_ms(kPollMs);
                if ((Hal::get_time_ms() - t0) >= kLongPressMs) { 
                    burst = true; 
                    break; 
                }
            }

            if (burst) {
                Hal::buzzer_long_beep();
                Hal::led_set_trigger_active(true);
                Hal::send_hid_report(static_cast<uint8_t>(HidKey::VolumeUp));
                Hal::sleep_ms(kBurstHoldMs);
                Hal::send_hid_report(0x00);
                Hal::led_set_trigger_active(false);
                while (Hal::is_button_held()) {
                    Hal::sleep_ms(kPollMs);
                }
            }

            Hal::sleep_ms(kDebounceMs);
        }
    }

    // Called from encoder polling timer when rotation is detected
    static void on_encoder_rotate(int steps) {
        if (steps > 0) {
            // Clockwise -> Volume Up
            Hal::send_hid_report(static_cast<uint8_t>(HidKey::VolumeUp));
            Hal::sleep_ms(30);
            Hal::send_hid_report(0x00);
        } else if (steps < 0) {
            // Counter-Clockwise -> Volume Down
            Hal::send_hid_report(static_cast<uint8_t>(HidKey::VolumeDown));
            Hal::sleep_ms(30);
            Hal::send_hid_report(0x00);
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

        // Short press → mute toggle
        Hal::send_hid_report(static_cast<uint8_t>(HidKey::Mute));
        Hal::sleep_ms(50);
        Hal::send_hid_report(0x00);
    }
};

} // namespace remote
