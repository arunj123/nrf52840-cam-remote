#include <gtest/gtest.h>
#include "battery_conversion.hpp"

using remote::raw_to_millivolts;
using remote::millivolts_to_percent;

// ─── Compile-time sanity checks (constexpr verification) ────────

static_assert(raw_to_millivolts(0) == 0, "Zero raw should be 0 mV");
static_assert(millivolts_to_percent(0) == 0, "0 mV should be 0%");
static_assert(millivolts_to_percent(4200) == 100, "4200 mV should be 100%");

// ─── raw_to_millivolts Tests ────────────────────────────────────

class RawToMillivoltsTest : public ::testing::Test {};

TEST_F(RawToMillivoltsTest, ZeroRawReturnsZero) {
    EXPECT_EQ(raw_to_millivolts(0), 0);
}

TEST_F(RawToMillivoltsTest, MaxRawReturnsFullScale) {
    // 10-bit ADC max = 1023 → should be close to 3600 mV
    EXPECT_EQ(raw_to_millivolts(1023), 3596);
}

TEST_F(RawToMillivoltsTest, NegativeRawClampedToZero) {
    EXPECT_EQ(raw_to_millivolts(-1), 0);
    EXPECT_EQ(raw_to_millivolts(-100), 0);
    EXPECT_EQ(raw_to_millivolts(INT16_MIN), 0);
}

TEST_F(RawToMillivoltsTest, MidRangeValue) {
    // 512 / 1024 * 3600 = 1800 mV
    EXPECT_EQ(raw_to_millivolts(512), 1800);
}

// ─── millivolts_to_percent Tests ────────────────────────────────

class MillivoltsToPercentTest : public ::testing::Test {};

TEST_F(MillivoltsToPercentTest, BelowMinReturnsZero) {
    EXPECT_EQ(millivolts_to_percent(0), 0);
    EXPECT_EQ(millivolts_to_percent(2999), 0);
    EXPECT_EQ(millivolts_to_percent(3000), 0);
}

TEST_F(MillivoltsToPercentTest, AboveMaxReturns100) {
    EXPECT_EQ(millivolts_to_percent(4200), 100);
    EXPECT_EQ(millivolts_to_percent(4500), 100);
    EXPECT_EQ(millivolts_to_percent(UINT16_MAX), 100);
}

TEST_F(MillivoltsToPercentTest, MidpointReturns50) {
    // Midpoint = 3000 + (4200-3000)/2 = 3600
    EXPECT_EQ(millivolts_to_percent(3600), 50);
}

TEST_F(MillivoltsToPercentTest, LinearInterpolation) {
    // 3120 → (120/1200)*100 = 10%
    EXPECT_EQ(millivolts_to_percent(3120), 10);
    // 3900 → (900/1200)*100 = 75%
    EXPECT_EQ(millivolts_to_percent(3900), 75);
}

TEST_F(MillivoltsToPercentTest, BoundaryJustAboveMin) {
    // 3001 → (1/1200)*100 = 0 (integer truncation)
    EXPECT_EQ(millivolts_to_percent(3001), 0);
}

TEST_F(MillivoltsToPercentTest, BoundaryJustBelowMax) {
    // 4199 → (1199/1200)*100 = 99
    EXPECT_EQ(millivolts_to_percent(4199), 99);
}
