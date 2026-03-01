/** @file
 *  @brief HoG Service class
 */

#pragma once

#include <zephyr/types.h>

namespace remote {

class HidService {
public:
    static void init();
    static void run_button_loop();
};

} // namespace remote
