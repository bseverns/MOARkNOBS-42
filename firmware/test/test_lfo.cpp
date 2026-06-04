#include "unity_config.h"
#include <unity.h>

#include "LFO/LFO.h"
#include "LFO/LFOClock.h"
#include "LFO/LFOManager.h"
#include "ConfigManager.h"
#include "MIDIHandler.h"
#include <cmath>

namespace {
constexpr float kEpsilon = 0.02f;

struct Stats {
    float mean = 0.0f;
    float rms = 0.0f;
    float min = 0.0f;
    float max = 0.0f;
};

Stats sampleLfo(LFO &lfo, float durationSeconds, float sampleRateHz) {
    // Collect mean/rms/min/max over a synthetic capture window.
    const int totalSamples = static_cast<int>(durationSeconds * sampleRateHz);
    Stats stats{};
    stats.min = 1.0f;
    stats.max = -1.0f;

    float sum = 0.0f;
    float sumSquares = 0.0f;
    float dt = 1.0f / sampleRateHz;

    for (int i = 0; i < totalSamples; ++i) {
        lfo.advanceFreeRun(dt);
        float value = lfo.value();
        sum += value;
        sumSquares += value * value;
        if (value < stats.min)
            stats.min = value;
        if (value > stats.max)
            stats.max = value;
    }

    stats.mean = sum / static_cast<float>(totalSamples);
    stats.rms = sqrtf(sumSquares / static_cast<float>(totalSamples));
    return stats;
}
} // namespace

void test_waveform_ranges() {
    // All shapes should stay within the nominal -1..1 range (with a tiny margin).
    LFO lfo;
    lfo.setDepth(1.0f);
    lfo.setBipolar(true);
    lfo.setFrequencyHz(1.0f);

    LFOShape shapes[] = {LFOShape::Sine,   LFOShape::Triangle,   LFOShape::Saw,
                         LFOShape::Square, LFOShape::SampleHold, LFOShape::RandomSlew};

    for (LFOShape shape : shapes) {
        lfo.setShape(shape);
        lfo.setPhase(0.0f);
        lfo.setRandomSeed(0x1234u);
        Stats stats = sampleLfo(lfo, 1.0f, 200.0f);
        TEST_ASSERT_TRUE(stats.min >= -1.05f);
        TEST_ASSERT_TRUE(stats.max <= 1.05f);
    }
}

void test_phase_continuity_free_run() {
    // Free-run phase should wrap smoothly around 1.0.
    LFO lfo;
    lfo.setShape(LFOShape::Sine);
    lfo.setFrequencyHz(1.0f);
    lfo.setDepth(1.0f);
    lfo.setBipolar(true);
    lfo.resetPhase();

    for (int i = 0; i < 1000; ++i) {
        lfo.advanceFreeRun(0.001f);
    }
    float phase = lfo.getPhase();
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, phase);
}

void test_sync_ticks_at_120_bpm() {
    // Clock sync should advance phase linearly with tick counts.
    LFO lfo;
    lfo.setShape(LFOShape::Saw);
    lfo.setDepth(1.0f);
    lfo.setBipolar(true);
    lfo.resetPhase();

    uint32_t ticksPerCycle = LFOClock::ticksPerCycle(LFOSyncRatio::Div1);
    lfo.advanceClockTicks(ticksPerCycle / 2, ticksPerCycle);
    TEST_ASSERT_FLOAT_WITHIN(kEpsilon, 0.5f, lfo.getPhase());
    lfo.advanceClockTicks(ticksPerCycle / 2, ticksPerCycle);
    TEST_ASSERT_FLOAT_WITHIN(kEpsilon, 0.0f, lfo.getPhase());
}

void test_div_mult_math() {
    // Verify clock ratio math for every division/multiplication.
    TEST_ASSERT_EQUAL_UINT32(24u, LFOClock::ticksPerCycle(LFOSyncRatio::Div1));
    TEST_ASSERT_EQUAL_UINT32(48u, LFOClock::ticksPerCycle(LFOSyncRatio::Div2));
    TEST_ASSERT_EQUAL_UINT32(96u, LFOClock::ticksPerCycle(LFOSyncRatio::Div4));
    TEST_ASSERT_EQUAL_UINT32(192u, LFOClock::ticksPerCycle(LFOSyncRatio::Div8));
    TEST_ASSERT_EQUAL_UINT32(384u, LFOClock::ticksPerCycle(LFOSyncRatio::Div16));
    TEST_ASSERT_EQUAL_UINT32(768u, LFOClock::ticksPerCycle(LFOSyncRatio::Div32));
    TEST_ASSERT_EQUAL_UINT32(12u, LFOClock::ticksPerCycle(LFOSyncRatio::Mul2));
    TEST_ASSERT_EQUAL_UINT32(6u, LFOClock::ticksPerCycle(LFOSyncRatio::Mul4));
}

void test_sample_hold_length() {
    // Sample/hold should keep the same value across a full cycle.
    LFO lfo;
    lfo.setShape(LFOShape::SampleHold);
    lfo.setFrequencyHz(1.0f);
    lfo.setDepth(1.0f);
    lfo.setBipolar(true);
    lfo.setRandomSeed(0x99u);
    lfo.resetPhase();

    lfo.advanceFreeRun(0.1f);
    float first = lfo.value();
    for (int i = 0; i < 5; ++i) {
        lfo.advanceFreeRun(0.1f);
        TEST_ASSERT_FLOAT_WITHIN(kEpsilon, first, lfo.value());
    }

    lfo.advanceFreeRun(0.6f);
    float next = lfo.value();
    TEST_ASSERT_TRUE(fabsf(next - first) > 0.05f);
}

void test_golden_stats() {
    // Golden statistical bands guard against waveform regressions.
    struct ShapeExpectation {
        LFOShape shape;
        float meanMin;
        float meanMax;
        float rmsMin;
        float rmsMax;
    };

    ShapeExpectation expectations[] = {
        {LFOShape::Sine, -0.02f, 0.02f, 0.69f, 0.72f},
        {LFOShape::Triangle, -0.03f, 0.03f, 0.55f, 0.60f},
        {LFOShape::Saw, -0.03f, 0.03f, 0.55f, 0.60f},
        {LFOShape::Square, -0.02f, 0.02f, 0.95f, 1.05f},
        {LFOShape::SampleHold, -0.2f, 0.2f, 0.4f, 0.8f},
        {LFOShape::RandomSlew, -0.2f, 0.2f, 0.3f, 0.7f},
    };

    for (const auto &expectation : expectations) {
        LFO lfo;
        lfo.setShape(expectation.shape);
        lfo.setFrequencyHz(1.0f);
        lfo.setDepth(1.0f);
        lfo.setBipolar(true);
        lfo.setRandomSeed(0xfaceu);
        lfo.resetPhase();
        Stats stats = sampleLfo(lfo, 2.0f, 1000.0f);
        TEST_ASSERT_TRUE(stats.mean >= expectation.meanMin);
        TEST_ASSERT_TRUE(stats.mean <= expectation.meanMax);
        TEST_ASSERT_TRUE(stats.rms >= expectation.rmsMin);
        TEST_ASSERT_TRUE(stats.rms <= expectation.rmsMax);
    }
}

void test_lfo_clock_consumes_ticks() {
    // LFOClock should mirror MIDI tick deltas without drifting or double-counting.
    MIDIHandler midi;
    LFOClock clock;
    clock.attach(&midi);

    for (int i = 0; i < 5; ++i) {
        midi.generateClockTick();
    }
    TEST_ASSERT_EQUAL_UINT32(5u, clock.consumeTickDelta());
    TEST_ASSERT_EQUAL_UINT32(0u, clock.consumeTickDelta());

    midi.generateClockTick();
    midi.generateClockTick();
    TEST_ASSERT_EQUAL_UINT32(2u, clock.consumeTickDelta());
}

void test_lfo_apply_profile_resets_sync_timing_baseline() {
    MIDIHandler midi;
    LFOManager manager;
    manager.attachMIDI(&midi);

    for (int i = 0; i < 12; ++i) {
        midi.generateClockTick();
    }

    ProfileData profile{};
    profile.lfos[0].shape = static_cast<uint8_t>(LFOShape::Saw);
    profile.lfos[0].frequencyHz = 1.0f;
    profile.lfos[0].depth = 1.0f;
    profile.lfos[0].bipolar = 0;
    profile.lfos[0].syncEnabled = 1;
    profile.lfos[0].syncRatio = static_cast<uint8_t>(LFOSyncRatio::Div1);

    manager.applyProfile(profile);
    manager.update(100);

    TEST_ASSERT_FLOAT_WITHIN(kEpsilon, 0.0f, manager.normalizedValue(0));
}

void test_lfo_slot_value_route_emits_slot_callback() {
    LFOManager manager;
    uint8_t observedSlot = 0xFF;
    uint8_t observedValue = 0;
    uint8_t callbackCount = 0;
    manager.setSlotValueCallback([&](uint8_t slotIndex, uint8_t value) {
        observedSlot = slotIndex;
        observedValue = value;
        callbackCount++;
    });

    LFO &lfo = manager.lfo(0);
    lfo.setShape(LFOShape::Square);
    lfo.setFrequencyHz(0.0f);
    lfo.setDepth(1.0f);
    lfo.setBipolar(false);
    lfo.setPhase(0.0f);
    manager.addSlotValueRoute(0, 7, 1.0f);

    manager.update(10);

    TEST_ASSERT_EQUAL_UINT8(1, callbackCount);
    TEST_ASSERT_EQUAL_UINT8(7, observedSlot);
    TEST_ASSERT_EQUAL_UINT8(127, observedValue);
}

void test_lfo_slot_value_route_applies_signed_amount_and_range() {
    LFOManager manager;
    uint8_t observedValue = 0;
    manager.setSlotValueCallback(
        [&](uint8_t /*slotIndex*/, uint8_t value) { observedValue = value; });

    LFO &lfo = manager.lfo(0);
    lfo.setShape(LFOShape::Square);
    lfo.setFrequencyHz(0.0f);
    lfo.setDepth(1.0f);
    lfo.setBipolar(false);
    lfo.setPhase(0.0f);
    manager.addSlotValueRoute(0, 7, 1.0f, -100, 10, 20);

    manager.update(10);

    TEST_ASSERT_EQUAL_UINT8(10, observedValue);
}
