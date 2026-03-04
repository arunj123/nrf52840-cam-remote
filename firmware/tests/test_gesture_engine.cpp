#include <gtest/gtest.h>
#include <vector>
#include "gesture_engine.hpp"

using remote::HidKey;

// Mock HAL state
struct MockState {
    uint64_t time_ms = 0;
    bool button_held = false;
    uint64_t release_time_ms = 0;
    std::vector<uint8_t> sent_reports;
    int beep_count = 0;
    bool led_active = false;

    void reset() {
        time_ms = 0;
        button_held = false;
        release_time_ms = 0;
        sent_reports.clear();
        beep_count = 0;
        led_active = false;
    }
};

static MockState state;

struct MockHal {
    static uint64_t get_time_ms() { return state.time_ms; }
    static void sleep_ms(int ms) { 
        state.time_ms += ms; 
        if (state.button_held && state.time_ms >= state.release_time_ms) {
            state.button_held = false;
        }
    }
    static bool is_button_held() { return state.button_held; }
    static void send_hid_report(uint8_t key_bits) { state.sent_reports.push_back(key_bits); }
    static void buzzer_long_beep() { state.beep_count++; }
    static void led_set_trigger_active(bool active) { state.led_active = active; }
};

using TestEngine = remote::GestureEngine<MockHal>;

class GestureEngineTest : public ::testing::Test {
protected:
    void SetUp() override {
        state.reset();
    }
    
    void simulate_main_button_press(int hold_duration_ms) {
        state.button_held = true;
        state.release_time_ms = state.time_ms + hold_duration_ms;
        TestEngine::on_main_button_wake(); // Blocks until button is "released" in sleep_ms
    }
};

TEST_F(GestureEngineTest, SingleClickSendsVolumeUp) {
    // Press for 100ms (more than debounce 60ms, less than long press 800ms)
    simulate_main_button_press(100);
    
    // Should see VolumeUp followed by 0x00
    ASSERT_EQ(state.sent_reports.size(), 2);
    EXPECT_EQ(state.sent_reports[0], static_cast<uint8_t>(HidKey::VolumeUp));
    EXPECT_EQ(state.sent_reports[1], 0x00);
    
    // No beep for short press
    EXPECT_EQ(state.beep_count, 0);
}

TEST_F(GestureEngineTest, GlitchIsIgnored) {
    // Press for 20ms (less than debounce 60ms)
    simulate_main_button_press(20);
    
    // No reports sent
    EXPECT_TRUE(state.sent_reports.empty());
}

TEST_F(GestureEngineTest, LongPressSendsBurst) {
    // Press for 1000ms (more than long press 800ms)
    simulate_main_button_press(1000);
    
    // Should see VolumeUp followed by 0x00 twice (once from single click, once from burst)
    // Wait, let's look at the logic: VolumeUp is sent initially on press, then AGAIN if long press triggers!
    ASSERT_EQ(state.sent_reports.size(), 4);
    EXPECT_EQ(state.sent_reports[0], static_cast<uint8_t>(HidKey::VolumeUp)); // Initial click
    EXPECT_EQ(state.sent_reports[1], 0x00);                                   // Initial click release
    EXPECT_EQ(state.sent_reports[2], static_cast<uint8_t>(HidKey::VolumeUp)); // Burst press
    EXPECT_EQ(state.sent_reports[3], 0x00);                                   // Burst release
    
    // Should beep once
    EXPECT_EQ(state.beep_count, 1);
}

TEST_F(GestureEngineTest, EncoderRotateClockwise) {
    TestEngine::on_encoder_rotate(1); 
    ASSERT_EQ(state.sent_reports.size(), 2);
    EXPECT_EQ(state.sent_reports[0], static_cast<uint8_t>(HidKey::VolumeUp));
    EXPECT_EQ(state.sent_reports[1], 0x00);
}

TEST_F(GestureEngineTest, EncoderRotateCounterClockwise) {
    TestEngine::on_encoder_rotate(-1); 
    ASSERT_EQ(state.sent_reports.size(), 2);
    EXPECT_EQ(state.sent_reports[0], static_cast<uint8_t>(HidKey::VolumeDown));
    EXPECT_EQ(state.sent_reports[1], 0x00);
}

TEST_F(GestureEngineTest, EncoderMuteButton) {
    TestEngine::on_encoder_button_press();
    ASSERT_EQ(state.sent_reports.size(), 2);
    EXPECT_EQ(state.sent_reports[0], static_cast<uint8_t>(HidKey::Mute));
    EXPECT_EQ(state.sent_reports[1], 0x00);
}
