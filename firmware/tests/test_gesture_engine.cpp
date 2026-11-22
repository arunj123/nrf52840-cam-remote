#include <gtest/gtest.h>
#include <vector>
#include "gesture_engine.hpp"

using remote::HidKey;

// Mock HAL state
struct MockState {
    uint64_t time_ms = 0;
    bool button_held = false;
    uint64_t release_time_ms = 0;
    bool secondary_button_held = false;
    uint64_t secondary_release_time_ms = 0;
    bool encoder_button_held = false;
    uint64_t encoder_release_time_ms = 0;
    std::vector<uint8_t> sent_reports;
    int beep_count = 0;
    bool led_active = false;
    int profile_switch_count = 0;
    int clear_bonds_count = 0;
    uint8_t active_profile = 0;
    remote::AllSlotMappings slot_mappings = {
        remote::kDefaultConfigMappings,
        remote::kDefaultConfigMappings,
        remote::kDefaultConfigMappings,
    };

    void reset() {
        time_ms = 0;
        button_held = false;
        release_time_ms = 0;
        secondary_button_held = false;
        secondary_release_time_ms = 0;
        encoder_button_held = false;
        encoder_release_time_ms = 0;
        sent_reports.clear();
        beep_count = 0;
        led_active = false;
        profile_switch_count = 0;
        clear_bonds_count = 0;
        active_profile = 0;
        slot_mappings = {
            remote::kDefaultConfigMappings,
            remote::kDefaultConfigMappings,
            remote::kDefaultConfigMappings,
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
        if (state.secondary_button_held && state.time_ms >= state.secondary_release_time_ms) {
            state.secondary_button_held = false;
        }
        if (state.encoder_button_held && state.time_ms >= state.encoder_release_time_ms) {
            state.encoder_button_held = false;
        }
    }
    static bool is_button_held() { return state.button_held; }
    static bool is_secondary_button_held() { return state.secondary_button_held; }
    static bool is_encoder_button_held() { return state.encoder_button_held; }
    static void send_hid_report(uint8_t key_bits) { state.sent_reports.push_back(key_bits); }
    static void buzzer_beep() { state.beep_count++; }
    static void buzzer_long_beep() { state.beep_count++; }
    static void led_set_trigger_active(bool active) { state.led_active = active; }
    static void on_profile_switch() { state.profile_switch_count++; }
    static void clear_bonds() { state.clear_bonds_count++; }
    static uint8_t get_active_profile() { return state.active_profile; }
    static remote::ConfigMappings get_config_mappings() {
        const uint8_t slot = (state.active_profile < remote::kNumHidSlots) ? state.active_profile : 0;
        return state.slot_mappings[slot];
    }
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

    void simulate_secondary_button_press(int hold_duration_ms) {
        state.secondary_button_held = true;
        state.secondary_release_time_ms = state.time_ms + hold_duration_ms;
        TestEngine::on_secondary_button_wake();
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
    state.active_profile = 1;
    state.slot_mappings[1].main_button_single = HidKey::NextTrack;
    simulate_main_button_press(100);
    ASSERT_EQ(state.sent_reports.size(), 2);
    EXPECT_EQ(state.sent_reports[0], static_cast<uint8_t>(HidKey::NextTrack));
}

TEST_F(GestureEngineTest, ConfigurableProfileEncoder) {
    state.active_profile = 2;
    state.slot_mappings[2].encoder_cw = HidKey::NextTrack;
    state.slot_mappings[2].encoder_ccw = HidKey::PrevTrack;
    
    TestEngine::on_encoder_rotate(-60); // CW
    EXPECT_EQ(state.sent_reports[0], static_cast<uint8_t>(HidKey::NextTrack));
    
    state.sent_reports.clear();
    TestEngine::on_encoder_rotate(60); // CCW
    EXPECT_EQ(state.sent_reports[0], static_cast<uint8_t>(HidKey::PrevTrack));
}

TEST_F(GestureEngineTest, ConfigurableProfileEncoderButton) {
    state.active_profile = 1;
    state.slot_mappings[1].encoder_button_short = HidKey::PlayPause;
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
    state.encoder_release_time_ms = state.time_ms + 440;
    TestEngine::on_encoder_button_press();
    ASSERT_EQ(state.sent_reports.size(), 2);
    EXPECT_EQ(state.sent_reports[0], static_cast<uint8_t>(HidKey::Mute));
    EXPECT_EQ(state.sent_reports[1], 0x00);
    EXPECT_EQ(state.profile_switch_count, 0);
}

TEST_F(GestureEngineTest, EncoderButtonLongPressSwitchesProfileAt500Ms) {
    state.encoder_button_held = true;
    state.encoder_release_time_ms = state.time_ms + 505;
    TestEngine::on_encoder_button_press();
    EXPECT_EQ(state.profile_switch_count, 1);
    EXPECT_TRUE(state.sent_reports.empty());
}

TEST_F(GestureEngineTest, MainButtonHold500MsTriggersBurst) {
    state.slot_mappings[0].main_button_single = HidKey::PlayPause;
    state.slot_mappings[0].main_button_long = HidKey::NextTrack;
    simulate_main_button_press(580); // 60ms debounce + 520ms hold >= 500ms
    ASSERT_EQ(state.sent_reports.size(), 2);
    EXPECT_EQ(state.sent_reports[0], static_cast<uint8_t>(HidKey::NextTrack));
    EXPECT_EQ(state.sent_reports[1], 0x00);
}

TEST_F(GestureEngineTest, MainButtonHold450MsTriggersSingleClick) {
    state.slot_mappings[0].main_button_single = HidKey::PlayPause;
    state.slot_mappings[0].main_button_long = HidKey::NextTrack;
    simulate_main_button_press(450); // 60ms debounce + 390ms hold < 500ms
    ASSERT_EQ(state.sent_reports.size(), 2);
    EXPECT_EQ(state.sent_reports[0], static_cast<uint8_t>(HidKey::PlayPause));
    EXPECT_EQ(state.sent_reports[1], 0x00);
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
    state.slot_mappings[0].main_button_single = HidKey::PlayPause;
    simulate_main_button_press(100);
    ASSERT_EQ(state.sent_reports.size(), 2);
    EXPECT_EQ(state.sent_reports[0], static_cast<uint8_t>(HidKey::PlayPause));
    EXPECT_EQ(state.sent_reports[1], 0x00);
}

TEST_F(GestureEngineTest, PerSlotMappingsAreIndependentAcrossSlots0To2) {
    state.slot_mappings[0].main_button_single = HidKey::VolumeUp;
    state.slot_mappings[1].main_button_single = HidKey::PlayPause;
    state.slot_mappings[2].main_button_single = HidKey::NextTrack;

    state.slot_mappings[0].encoder_cw = HidKey::VolumeUp;
    state.slot_mappings[1].encoder_cw = HidKey::NextTrack;
    state.slot_mappings[2].encoder_cw = HidKey::PrevTrack;

    state.slot_mappings[0].encoder_button_short = HidKey::Mute;
    state.slot_mappings[1].encoder_button_short = HidKey::VolumeDown;
    state.slot_mappings[2].encoder_button_short = HidKey::PlayPause;

    for (uint8_t s = 0; s < remote::kNumHidSlots; ++s) {
        state.active_profile = s;
        state.sent_reports.clear();
        simulate_main_button_press(100);
        ASSERT_EQ(state.sent_reports.size(), 2);
        EXPECT_EQ(state.sent_reports[0], static_cast<uint8_t>(state.slot_mappings[s].main_button_single));

        state.sent_reports.clear();
        TestEngine::on_encoder_rotate(-60);
        ASSERT_EQ(state.sent_reports.size(), 2);
        EXPECT_EQ(state.sent_reports[0], static_cast<uint8_t>(state.slot_mappings[s].encoder_cw));

        state.sent_reports.clear();
        state.encoder_button_held = true;
        state.encoder_release_time_ms = state.time_ms + 100;
        TestEngine::on_encoder_button_press();
        ASSERT_EQ(state.sent_reports.size(), 2);
        EXPECT_EQ(state.sent_reports[0], static_cast<uint8_t>(state.slot_mappings[s].encoder_button_short));
    }
}

TEST_F(GestureEngineTest, HidKeyNoneSendsNoReport) {
    state.slot_mappings[0].main_button_single = HidKey::None;
    simulate_main_button_press(100);
    EXPECT_TRUE(state.sent_reports.empty());
}

TEST_F(GestureEngineTest, SecondaryButtonShortClickSendsPlayPause) {
    simulate_secondary_button_press(100);
    ASSERT_EQ(state.sent_reports.size(), 2);
    EXPECT_EQ(state.sent_reports[0], static_cast<uint8_t>(HidKey::PlayPause));
    EXPECT_EQ(state.sent_reports[1], 0x00);
    EXPECT_EQ(state.beep_count, 1);
    EXPECT_FALSE(state.led_active);
}

TEST_F(GestureEngineTest, SecondaryButtonLongPressSendsNextTrack) {
    simulate_secondary_button_press(520);
    ASSERT_EQ(state.sent_reports.size(), 2);
    EXPECT_EQ(state.sent_reports[0], static_cast<uint8_t>(HidKey::NextTrack));
    EXPECT_EQ(state.sent_reports[1], 0x00);
    EXPECT_EQ(state.beep_count, 1);
    EXPECT_FALSE(state.led_active);
}

TEST_F(GestureEngineTest, SecondaryButtonGlitchIsIgnored) {
    simulate_secondary_button_press(30);
    EXPECT_TRUE(state.sent_reports.empty());
    EXPECT_EQ(state.beep_count, 0);
}

TEST_F(GestureEngineTest, ConfigurableProfileSecondaryButtonAcrossSlots) {
    state.slot_mappings[0].secondary_button_single = HidKey::PlayPause;
    state.slot_mappings[1].secondary_button_single = HidKey::VolumeDown;
    state.slot_mappings[2].secondary_button_single = HidKey::Mute;

    state.slot_mappings[0].secondary_button_long = HidKey::NextTrack;
    state.slot_mappings[1].secondary_button_long = HidKey::PrevTrack;
    state.slot_mappings[2].secondary_button_long = HidKey::VolumeUp;

    for (uint8_t s = 0; s < remote::kNumHidSlots; ++s) {
        state.active_profile = s;
        state.sent_reports.clear();
        simulate_secondary_button_press(100);
        ASSERT_EQ(state.sent_reports.size(), 2);
        EXPECT_EQ(state.sent_reports[0], static_cast<uint8_t>(state.slot_mappings[s].secondary_button_single));

        state.sent_reports.clear();
        simulate_secondary_button_press(520);
        ASSERT_EQ(state.sent_reports.size(), 2);
        EXPECT_EQ(state.sent_reports[0], static_cast<uint8_t>(state.slot_mappings[s].secondary_button_long));
    }
}

TEST_F(GestureEngineTest, SecondaryButtonHold5sDoesNotClearBonds) {
    simulate_secondary_button_press(5100);
    EXPECT_EQ(state.clear_bonds_count, 0);
    ASSERT_EQ(state.sent_reports.size(), 2);
    EXPECT_EQ(state.sent_reports[0], static_cast<uint8_t>(HidKey::NextTrack));
    EXPECT_EQ(state.sent_reports[1], 0x00);
}

TEST_F(GestureEngineTest, ValidHidKeyChecksAllCategoriesAndRejectsUndefined) {
    constexpr HidKey valid_keys[] = {
        HidKey::None,
        HidKey::VolumeUp, HidKey::VolumeDown, HidKey::Mute, HidKey::PlayPause,
        HidKey::NextTrack, HidKey::PrevTrack, HidKey::BrowserBack, HidKey::BrowserForward,
        HidKey::ScrollUp, HidKey::ScrollDown, HidKey::ScrollUpFast, HidKey::ScrollDownFast,
        HidKey::ScrollLeft, HidKey::ScrollRight,
        HidKey::MouseLeftClick, HidKey::MouseRightClick, HidKey::MouseMiddleClick,
        HidKey::KeySpace, HidKey::KeyEnter, HidKey::KeyEscape, HidKey::KeyTab,
        HidKey::KeyBackspace, HidKey::KeyUp, HidKey::KeyDown, HidKey::KeyLeft,
        HidKey::KeyRight, HidKey::KeyPageUp, HidKey::KeyPageDown, HidKey::KeyHome,
        HidKey::KeyEnd, HidKey::KeyF5, HidKey::KeyFullscreen, HidKey::KeyPrintScreen,
        HidKey::ShortcutCopy, HidKey::ShortcutPaste, HidKey::ShortcutCut,
        HidKey::ShortcutUndo, HidKey::ShortcutRedo, HidKey::ShortcutSelectAll,
        HidKey::ShortcutSave, HidKey::ShortcutFind, HidKey::ShortcutZoomIn,
        HidKey::ShortcutZoomOut, HidKey::ShortcutZoomReset, HidKey::ShortcutNewTab,
        HidKey::ShortcutCloseTab, HidKey::ShortcutReopenTab, HidKey::ShortcutNextTab,
        HidKey::ShortcutPrevTab, HidKey::ShortcutAltTab, HidKey::ShortcutShowDesktop,
        HidKey::ShortcutLockScreen, HidKey::ShortcutSnippingTool,
    };
    for (HidKey key : valid_keys) {
        EXPECT_TRUE(remote::is_valid_hid_key(static_cast<uint8_t>(key)))
            << "Expected HidKey 0x" << std::hex << static_cast<int>(key) << " to be valid";
    }

    EXPECT_FALSE(remote::is_valid_hid_key(0x03));
    EXPECT_FALSE(remote::is_valid_hid_key(0x2A));
    EXPECT_FALSE(remote::is_valid_hid_key(0x2F));
    EXPECT_FALSE(remote::is_valid_hid_key(0x55));
    EXPECT_FALSE(remote::is_valid_hid_key(0xFF));
}

TEST_F(GestureEngineTest, DispatchesScrollAndKeyboardShortcuts) {
    state.slot_mappings[0].encoder_cw = HidKey::ScrollUp;
    state.slot_mappings[0].encoder_ccw = HidKey::ScrollDown;
    state.slot_mappings[0].main_button_single = HidKey::ShortcutCopy;
    state.slot_mappings[0].secondary_button_single = HidKey::ShortcutZoomIn;
    state.slot_mappings[0].encoder_button_short = HidKey::KeySpace;

    TestEngine::on_encoder_rotate(-60);
    ASSERT_EQ(state.sent_reports.size(), 2);
    EXPECT_EQ(state.sent_reports[0], static_cast<uint8_t>(HidKey::ScrollUp));
    EXPECT_EQ(state.sent_reports[1], 0x00);

    state.sent_reports.clear();
    TestEngine::on_encoder_rotate(60);
    ASSERT_EQ(state.sent_reports.size(), 2);
    EXPECT_EQ(state.sent_reports[0], static_cast<uint8_t>(HidKey::ScrollDown));
    EXPECT_EQ(state.sent_reports[1], 0x00);

    state.sent_reports.clear();
    simulate_main_button_press(100);
    ASSERT_EQ(state.sent_reports.size(), 2);
    EXPECT_EQ(state.sent_reports[0], static_cast<uint8_t>(HidKey::ShortcutCopy));
    EXPECT_EQ(state.sent_reports[1], 0x00);

    state.sent_reports.clear();
    simulate_secondary_button_press(100);
    ASSERT_EQ(state.sent_reports.size(), 2);
    EXPECT_EQ(state.sent_reports[0], static_cast<uint8_t>(HidKey::ShortcutZoomIn));
    EXPECT_EQ(state.sent_reports[1], 0x00);

    state.sent_reports.clear();
    state.encoder_button_held = true;
    state.encoder_release_time_ms = state.time_ms + 100;
    TestEngine::on_encoder_button_press();
    ASSERT_EQ(state.sent_reports.size(), 2);
    EXPECT_EQ(state.sent_reports[0], static_cast<uint8_t>(HidKey::KeySpace));
    EXPECT_EQ(state.sent_reports[1], 0x00);
}
