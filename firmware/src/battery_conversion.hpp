#pragma once

/**
 * @file battery_conversion.hpp
 * @brief Pure conversion functions for battery voltage mapping.
 *
 * Extracted from battery.cpp so they can be unit-tested on the host
 * without any Zephyr or hardware dependencies.
 */

#include <cstdint>

namespace remote {

/// ADC resolution (10-bit)
inline constexpr uint16_t kAdcResolution = 10;

/// Convert a raw ADC sample to millivolts (assuming 3.6V reference, 10-bit).
constexpr uint16_t raw_to_millivolts(int16_t raw) noexcept {
    if (raw < 0) raw = 0;
    return static_cast<uint16_t>(
        (static_cast<uint32_t>(raw) * 3600) / (1 << kAdcResolution)
    );
}

/// Map millivolts to a 0–100 % battery level (linear between 3.0V–4.2V).
constexpr uint8_t millivolts_to_percent(uint16_t mv) noexcept {
    constexpr uint16_t kMinMv = 3000;
    constexpr uint16_t kMaxMv = 4200;

    if (mv <= kMinMv) return 0;
    if (mv >= kMaxMv) return 100;

    return static_cast<uint8_t>(
        (static_cast<uint32_t>(mv - kMinMv) * 100) / (kMaxMv - kMinMv)
    );
}

} // namespace remote
