#include <gtest/gtest.h>
#include <vector>
#include "gesture_engine.hpp"

using remote::HidKey;

// Mock HAL state
struct MockState {
    uint64_t time_ms = 0;
    bool button_held = false;
    uint64_t release_time_ms = 0;
    bool encoder_button_held = false;
    uint64_t encoder_release_time_ms = 0;
    std::vector<uint8_t> sent_reports;
    int beep_count = 0;
    bool led_active = false;
    int profile_switch_count = 0;
    int clear_bonds_count = 0;
    uint8_t active_profile = 0;
    remote::ConfigMappings mappings = {
        .main_button_single = HidKey::VolumeUp,
        .main_button_long = HidKey::VolumeUp,
        .encoder_cw = HidKey::VolumeUp,
        .encoder_ccw = HidKey::VolumeDown,
        .encoder_button_short = HidKey::Mute,
    };

    void reset() {
        time_ms = 0;
        button_held = false;
        release_time_ms = 0;
        encoder_button_held = false;
        encoder_release_time_ms = 0;
        sent_reports.clear();
        beep_count = 0;
        led_active = false;
        profile_switch_count = 0;
        clear_bonds_count = 0;
        active_profile = 0;
        mappings = {
            .main_button_single = HidKey::VolumeUp,
            .main_button_long = HidKey::VolumeUp,
            .encoder_cw = HidKey::VolumeUp,
            .encoder_ccw = HidKey::VolumeDown,
            .encoder_button_short = HidKey::Mute,
        };
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
        if (state.encoder_button_held && state.time_ms >= state.encoder_release_time_ms) {
            state.encoder_button_held = false;
        }
    }
    static bool is_button_held() { return state.button_held; }
    static bool is_encoder_button_held() { return state.encoder_button_held; }
    static void send_hid_report(uint8_t key_bits) { state.sent_reports.push_back(key_bits); }
    static void buzzer_beep() { state.beep_count++; }
    static void buzzer_long_beep() { state.beep_count++; }
    static void led_set_trigger_active(bool active) { state.led_active = active; }
    static void on_profile_switch() { state.profile_switch_count++; }
    static void clear_bonds() { state.clear_bonds_count++; }
    static uint8_t get_active_profile() { return state.active_profile; }
    static remote::ConfigMappings get_config_mappings() { return state.mappings; }
};

using TestEngine = remote::GestureEngine<MockHal>;

class GestureEngineTest : public ::testing::Test {
protected:
    void SetUp() override {
        state.reset();
        TestEngine::reset();
    }
    
    void simulate_main_button_press(int hold_duration_ms) {
        state.button_held = true;
        state.release_time_ms = state.time_ms + hold_duration_ms;
        TestEngine::on_main_button_wake();
    }
};

TEST_F(GestureEngineTest, SingleClickSendsVolumeUp) {
    simulate_main_button_press(100);
    ASSERT_EQ(state.sent_reports.size(), 2);
    EXPECT_EQ(state.sent_reports[0], static_cast<uint8_t>(HidKey::VolumeUp));
    EXPECT_EQ(state.sent_reports[1], 0x00);
    EXPECT_EQ(state.beep_count, 1);
}

TEST_F(GestureEngineTest, LongPressSendsBurst) {
    simulate_main_button_press(1000);
    ASSERT_EQ(state.sent_reports.size(), 2);
    EXPECT_EQ(state.sent_reports[0], static_cast<uint8_t>(HidKey::VolumeUp));
    EXPECT_EQ(state.sent_reports[1], 0x00);
    EXPECT_EQ(state.beep_count, 1);
}

TEST_F(GestureEngineTest, EncoderRotateClockwise) {
    TestEngine::on_encoder_rotate(-60); // Negative = CW
    ASSERT_EQ(state.sent_reports.size(), 2);
    EXPECT_EQ(state.sent_reports[0], static_cast<uint8_t>(HidKey::VolumeUp));
    EXPECT_EQ(state.sent_reports[1], 0x00);
}

TEST_F(GestureEngineTest, EncoderRotateCounterClockwise) {
    TestEngine::on_encoder_rotate(60); // Positive = CCW
    ASSERT_EQ(state.sent_reports.size(), 2);
    EXPECT_EQ(state.sent_reports[0], static_cast<uint8_t>(HidKey::VolumeDown));
    EXPECT_EQ(state.sent_reports[1], 0x00);
}

TEST_F(GestureEngineTest, ConfigurableProfileSingleClick) {
    state.active_profile = 3;
    state.mappings.main_button_single = HidKey::NextTrack;
    simulate_main_button_press(100);
    ASSERT_EQ(state.sent_reports.size(), 2);
    EXPECT_EQ(state.sent_reports[0], static_cast<uint8_t>(HidKey::NextTrack));
}

TEST_F(GestureEngineTest, ConfigurableProfileEncoder) {
    state.active_profile = 3;
    state.mappings.encoder_cw = HidKey::NextTrack;
    state.mappings.encoder_ccw = HidKey::PrevTrack;
    
    TestEngine::on_encoder_rotate(-60); // CW
    EXPECT_EQ(state.sent_reports[0], static_cast<uint8_t>(HidKey::NextTrack));
    
    state.sent_reports.clear();
    TestEngine::on_encoder_rotate(60); // CCW
    EXPECT_EQ(state.sent_reports[0], static_cast<uint8_t>(HidKey::PrevTrack));
}

TEST_F(GestureEngineTest, ConfigurableProfileEncoderButton) {
    state.active_profile = 3;
    state.mappings.encoder_button_short = HidKey::PlayPause;
    state.encoder_button_held = true;
    state.encoder_release_time_ms = state.time_ms + 100;
    TestEngine::on_encoder_button_press();
    ASSERT_EQ(state.sent_reports.size(), 2);
    EXPECT_EQ(state.sent_reports[0], static_cast<uint8_t>(HidKey::PlayPause));
}

TEST_F(GestureEngineTest, ShortGlitchIsIgnored) {
    simulate_main_button_press(30);
    EXPECT_TRUE(state.sent_reports.empty());
    EXPECT_EQ(state.beep_count, 0);
}

TEST_F(GestureEngineTest, FiveSecondHoldClearsBonds) {
    simulate_main_button_press(5100);
    EXPECT_EQ(state.clear_bonds_count, 1);
    EXPECT_TRUE(state.sent_reports.empty());
}

TEST_F(GestureEngineTest, EncoderButtonShortPressSendsMute) {
    state.encoder_button_held = true;
    state.encoder_release_time_ms = state.time_ms + 100;
    TestEngine::on_encoder_button_press();
    ASSERT_EQ(state.sent_reports.size(), 2);
    EXPECT_EQ(state.sent_reports[0], static_cast<uint8_t>(HidKey::Mute));
    EXPECT_EQ(state.sent_reports[1], 0x00);
}

TEST_F(GestureEngineTest, EncoderButtonLongPressSwitchesProfile) {
    state.encoder_button_held = true;
    state.encoder_release_time_ms = state.time_ms + 900;
    TestEngine::on_encoder_button_press();
    EXPECT_EQ(state.profile_switch_count, 1);
    EXPECT_TRUE(state.sent_reports.empty());
}

TEST_F(GestureEngineTest, EncoderSubDetentAccumulation) {
    TestEngine::on_encoder_rotate(30);
    EXPECT_TRUE(state.sent_reports.empty());

    TestEngine::on_encoder_rotate(30);
    ASSERT_EQ(state.sent_reports.size(), 2);
    EXPECT_EQ(state.sent_reports[0], static_cast<uint8_t>(HidKey::VolumeDown));
    EXPECT_EQ(state.sent_reports[1], 0x00);
}

TEST_F(GestureEngineTest, EncoderFastSpinMultipleDetents) {
    TestEngine::on_encoder_rotate(-120);
    ASSERT_EQ(state.sent_reports.size(), 4);
    EXPECT_EQ(state.sent_reports[0], static_cast<uint8_t>(HidKey::VolumeUp));
    EXPECT_EQ(state.sent_reports[1], 0x00);
    EXPECT_EQ(state.sent_reports[2], static_cast<uint8_t>(HidKey::VolumeUp));
    EXPECT_EQ(state.sent_reports[3], 0x00);
}

TEST_F(GestureEngineTest, CustomMappingsApplyOnHidSlot0) {
    state.active_profile = 0;
    state.mappings.main_button_single = HidKey::PlayPause;
    simulate_main_button_press(100);
    ASSERT_EQ(state.sent_reports.size(), 2);
    EXPECT_EQ(state.sent_reports[0], static_cast<uint8_t>(HidKey::PlayPause));
    EXPECT_EQ(state.sent_reports[1], 0x00);
}

TEST_F(GestureEngineTest, HidKeyNoneSendsNoReport) {
    state.mappings.main_button_single = HidKey::None;
    simulate_main_button_press(100);
    EXPECT_TRUE(state.sent_reports.empty());
}
