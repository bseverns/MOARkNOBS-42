#include "unity_config.h"
#include <unity.h>

#include "TestHelpers.h"
#include "BoardPowerProfile.h"
#include "LedAnimator.h"

void test_led_animator_cycles_mode_order() {
    LEDManager ledManager = createLEDManager();
    LedAnimator animator(ledManager);

    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(LedMode::Static),
                            static_cast<uint8_t>(animator.getMode()));

    animator.cycleMode();
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(LedMode::PeakHold),
                            static_cast<uint8_t>(animator.getMode()));

    animator.cycleMode();
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(LedMode::Trail),
                            static_cast<uint8_t>(animator.getMode()));

    animator.cycleMode();
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(LedMode::ClockPulse),
                            static_cast<uint8_t>(animator.getMode()));

    animator.cycleMode();
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(LedMode::Static),
                            static_cast<uint8_t>(animator.getMode()));
}

void test_led_animator_invalid_mode_falls_back_to_static() {
    LEDManager ledManager = createLEDManager();
    LedAnimator animator(ledManager);

    animator.setMode(static_cast<LedMode>(255));
    animator.tick(100, false, true);
    animator.tick(2600, false, true);

    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(LedMode::Static),
                            static_cast<uint8_t>(animator.getMode()));
}

void test_led_manager_clamps_brightness_to_board_power_profile() {
    LEDManager ledManager = createLEDManager();

    ledManager.setBrightness(255);

    TEST_ASSERT_EQUAL_UINT8(BoardPowerProfile::kLedBrightnessCap, ledManager.getBrightness());
}
