/** @file
 *  @brief Battery Monitor class
 */

#pragma once

#include <cstdint>

namespace remote {

class BatteryMonitor {
public:
    /// Initialize the SAADC for VDD measurement
    static void init();

    /// Read current battery level (0-100%)
    static uint8_t read_level();

    /// Start periodic battery level updates (every 60 seconds)
    static void start_periodic();
};

} // namespace remote
