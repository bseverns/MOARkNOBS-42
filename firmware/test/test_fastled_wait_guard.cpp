#include "unity_config.h"
#include <unity.h>

#include <FastLED/platforms/arm/mxrt1062/wait_time_utils.h>

// The Teensy 4 clockless drivers bail out when interrupts hog the bus for too
// long. Historically, subtracting INTERRUPT_THRESHOLD from WAIT_TIME with
// unsigned math wrapped around to a monster timeout and effectively disabled
// that safety valve. These checks make sure the new signed clamp keeps the
// guard dog awake.

void test_wait_guard_clamps_when_threshold_exceeds_wait() {
    constexpr int wait_time_us = 20;
    constexpr int interrupt_threshold_us = 40;

    const int32_t clamped =
        fastled_mxrt1062::clampWaitTimeDeltaUs(wait_time_us, interrupt_threshold_us);

    TEST_ASSERT_EQUAL_INT32(0, clamped);
    TEST_ASSERT_EQUAL_UINT32(0u, static_cast<uint32_t>(clamped) * 1000u);
}

void test_wait_guard_preserves_positive_delta() {
    constexpr int wait_time_us = 80;
    constexpr int interrupt_threshold_us = 40;

    const int32_t clamped =
        fastled_mxrt1062::clampWaitTimeDeltaUs(wait_time_us, interrupt_threshold_us);

    TEST_ASSERT_EQUAL_INT32(wait_time_us - interrupt_threshold_us, clamped);
}
