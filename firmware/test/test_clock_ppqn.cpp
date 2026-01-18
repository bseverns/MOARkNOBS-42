#include "unity_config.h"
#include <unity.h>

#include "ClockDiscipline.h"
#include <cmath>

void test_clock_ppqn_start_stop_continue() {
    ClockDiscipline clock;
    uint32_t tickCount = 0;
    unsigned long nowMs = 0;

    clock.observe(tickCount, nowMs, false);
    TEST_ASSERT_FALSE(clock.justResumed());

    clock.observe(tickCount, nowMs, true);
    TEST_ASSERT_TRUE(clock.justResumed());
    clock.clearResumeFlag();

    nowMs += 20;
    tickCount += 1;
    clock.observe(tickCount, nowMs, true);
    uint32_t consumed = clock.consumeTicks();
    TEST_ASSERT_EQUAL_UINT32(1, consumed);
    TEST_ASSERT_FALSE(clock.justResumed());

    clock.observe(tickCount, nowMs, false);
    nowMs += 10;
    tickCount += 1;
    clock.observe(tickCount, nowMs, false);
    TEST_ASSERT_EQUAL_UINT32(0, clock.consumeTicks());

    clock.observe(tickCount, nowMs, true);
    TEST_ASSERT_TRUE(clock.justResumed());
    clock.clearResumeFlag();

    nowMs += 200;
    tickCount += 1;
    clock.observe(tickCount, nowMs, true);
    clock.consumeTicks();
    TEST_ASSERT_TRUE(clock.driftDetected());
    clock.clearDriftFlag();
}

void test_clock_ppqn_timing_accuracy_with_tempo_jump() {
    ClockDiscipline clock;
    uint32_t tickCount = 0;
    double virtualMs = 0.0;
    const int stableTicks = 1440;
    const int totalTicks = 2880;
    double currentTickMs = 60000.0 / (120.0 * 24.0); // 20.83 ms per PPQN at 120 BPM
    const double stableTickMs = currentTickMs;

    clock.observe(tickCount, static_cast<unsigned long>(virtualMs), true);

    for (int i = 0; i < totalTicks; ++i) {
        if (i == stableTicks) {
            currentTickMs = 60000.0 / (90.0 * 24.0); // tempo jump to 90 BPM
        }
        tickCount += 1;
        double jitter = ((i % 5) - 2) * 0.35;
        double delta = currentTickMs + jitter;
        if (i == stableTicks - 1) {
            delta += stableTickMs * 2.5; // force a large gap to trigger drift detection
        }
        virtualMs += delta;
        clock.observe(tickCount, static_cast<unsigned long>(virtualMs), true);
        clock.consumeTicks();

        if (i >= 200) {
            double target = (i < stableTicks) ? stableTickMs : currentTickMs;
            double error = fabsf(clock.msPerTick() - static_cast<float>(target));
            TEST_ASSERT_TRUE_MESSAGE(error <= 1.0f, "msPerTick drift exceeded 1ms");
        }
        if (i == stableTicks - 1) {
            TEST_ASSERT_TRUE(clock.driftDetected());
            clock.clearDriftFlag();
        }
    }

    double finalExpected = 60000.0 / (90.0 * 24.0);
    double finalError = fabsf(clock.msPerTick() - static_cast<float>(finalExpected));
    TEST_ASSERT_TRUE_MESSAGE(finalError <= 1.0f, "final tempo estimate drifted too far");
}
