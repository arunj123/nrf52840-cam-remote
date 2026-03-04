cmake_minimum_required(VERSION 3.14)
project(nrf_control_tests)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()

# ─── GestureEngine Tests ─────────────────────────────────────────
add_executable(test_gesture_engine
  test_gesture_engine.cpp
)
target_include_directories(test_gesture_engine PRIVATE ../src)
target_link_libraries(test_gesture_engine
  GTest::gtest_main
)

# ─── Battery Conversion Tests ────────────────────────────────────
add_executable(test_battery_conversion
  test_battery_conversion.cpp
)
target_include_directories(test_battery_conversion PRIVATE ../src)
target_link_libraries(test_battery_conversion
  GTest::gtest_main
)

# ─── ProfileManager Tests ────────────────────────────────────────
add_executable(test_profile_manager
  test_profile_manager.cpp
)
target_include_directories(test_profile_manager PRIVATE ../src)
target_link_libraries(test_profile_manager
  GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(test_gesture_engine)
gtest_discover_tests(test_battery_conversion)
gtest_discover_tests(test_profile_manager)
