#include <gtest/gtest.h>
#include "battery_conversion.hpp"

using remote::raw_to_millivolts;
using remote::millivolts_to_percent;

// ─── Compile-time sanity checks (constexpr verification) ────────

static_assert(raw_to_millivolts(0) == 0, "Zero raw should be 0 mV");
static_assert(raw_to_millivolts(312) == 4189, "Calibration point raw=312 should map to 4189 mV");
static_assert(raw_to_millivolts(INT16_MAX) == 54986, "Oversized raw should clamp to 12-bit max");
static_assert(millivolts_to_percent(0) == 0, "0 mV should be 0%");
static_assert(millivolts_to_percent(4200) == 100, "4200 mV should be 100%");

// ─── raw_to_millivolts Tests ────────────────────────────────────

class RawToMillivoltsTest : public ::testing::Test {};

TEST_F(RawToMillivoltsTest, ZeroRawReturnsZero) {
    EXPECT_EQ(raw_to_millivolts(0), 0);
}

TEST_F(RawToMillivoltsTest, CalibratedBatteryPoint) {
    // 12-bit calibrated formula: (312 * 55000) / 4096 = 4189 mV (~4.2V full Li-Po)
    EXPECT_EQ(raw_to_millivolts(312), 4189);
}

TEST_F(RawToMillivoltsTest, Max12BitRawAndOverflowClamping) {
    // 12-bit ADC max = 4095 -> (4095 * 55000) / 4096 = 54986 mV
    EXPECT_EQ(raw_to_millivolts(4095), 54986);
    EXPECT_EQ(raw_to_millivolts(INT16_MAX), 54986);
}

TEST_F(RawToMillivoltsTest, NegativeRawClampedToZero) {
    EXPECT_EQ(raw_to_millivolts(-1), 0);
    EXPECT_EQ(raw_to_millivolts(-100), 0);
    EXPECT_EQ(raw_to_millivolts(INT16_MIN), 0);
}

TEST_F(RawToMillivoltsTest, MidRangeValue) {
    // (256 * 55000) / 4096 = 3437 mV
    EXPECT_EQ(raw_to_millivolts(256), 3437);
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
