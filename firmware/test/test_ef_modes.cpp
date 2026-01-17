#include "unity_config.h"
#include <unity.h>

#include "EnvelopeFollower.h"
#include "Hardware/IO.h"
#include "PotentiometerManager.h"
#include <cmath>
#include "TimeStub.h"

// Fake time base used by the EnvelopeFollower in test runs.
unsigned long g_fakeNowMs = 0;
static int g_analogValue = 0;

int analogProvider(uint8_t) { return g_analogValue; }

// Advance the fake millisecond clock.
void advanceMs(unsigned long ms) { g_fakeNowMs += ms; }

int adcFromVoltage(float volts) {
    float clamped = fmaxf(0.0f, fminf(volts, 3.3f));
    return static_cast<int>(lroundf(clamped / VadcScale));
}

EnvelopeFollower::EfModeSettings baseSettings(EnvelopeFollower::EFMode mode) {
    EnvelopeFollower::EfModeSettings settings{};
    settings.mode = mode;
    settings.autoBaseline = false;
    settings.autoGain = false;
    settings.attackMs = 5;
    settings.releaseMs = 50;
    settings.rmsWindowMs = 60;
    settings.gateThreshold = 20;
    settings.gateHysteresis = 5;
    settings.activityThreshold = 4;
    settings.gainTarget = 102;
    settings.baselineTauMs = 1000;
    settings.gainTauMs = 1500;
    return settings;
}
// Override the global time source for deterministic tests.
unsigned long now() { return g_fakeNowMs; }

struct EfHarnessGuard {
    EfHarnessGuard() {
        // Load the fake ADC provider and scrub time so each test starts clean.
        hardware::setAnalogReadProvider(analogProvider);
        g_fakeNowMs = 0;
        g_analogValue = 0;
    }
    ~EfHarnessGuard() { hardware::resetAnalogReadProvider(); }
};

void test_peak_mode_rises_and_falls() {
    EfHarnessGuard guard;
    PotentiometerManager pm(primaryMuxPins, secondaryMuxPins, potMuxAnalogPin);
    EnvelopeFollower ef(A0, &pm, 0);
    ef.setVref(1.65f);
    ef.setModeSettings(baseSettings(EnvelopeFollower::EFMode::Peak));

    g_analogValue = adcFromVoltage(1.65f);
    advanceMs(5);
    ef.update();
    int baseline = ef.getEnvelopeLevel();

    g_analogValue = adcFromVoltage(2.15f);
    for (int i = 0; i < 3; ++i) {
        advanceMs(5);
        ef.update();
    }
    int rise = ef.getEnvelopeLevel();
    TEST_ASSERT_TRUE(rise > baseline);

    g_analogValue = adcFromVoltage(1.65f);
    advanceMs(5);
    ef.update();
    int afterDrop = ef.getEnvelopeLevel();
    TEST_ASSERT_TRUE(afterDrop > 0);
    advanceMs(50);
    ef.update();
    int afterRelease = ef.getEnvelopeLevel();
    TEST_ASSERT_TRUE(afterRelease < afterDrop);
}

void test_rms_mode_converges() {
    EfHarnessGuard guard;
    PotentiometerManager pm(primaryMuxPins, secondaryMuxPins, potMuxAnalogPin);
    EnvelopeFollower ef(A0, &pm, 0);
    ef.setVref(1.65f);
    ef.setModeSettings(baseSettings(EnvelopeFollower::EFMode::RMS));

    g_analogValue = adcFromVoltage(2.15f);
    for (int i = 0; i < 40; ++i) {
        advanceMs(5);
        ef.update();
    }
    int level = ef.getEnvelopeLevel();
    TEST_ASSERT_TRUE(level > 20);
    TEST_ASSERT_TRUE(level < 80);
}

void test_gate_mode_hysteresis() {
    EfHarnessGuard guard;
    PotentiometerManager pm(primaryMuxPins, secondaryMuxPins, potMuxAnalogPin);
    EnvelopeFollower ef(A0, &pm, 0);
    ef.setVref(1.65f);
    EnvelopeFollower::EfModeSettings settings = baseSettings(EnvelopeFollower::EFMode::Gate);
    settings.gateThreshold = 30;
    settings.gateHysteresis = 6;
    ef.setModeSettings(settings);

    g_analogValue = adcFromVoltage(1.70f);
    advanceMs(5);
    ef.update();
    TEST_ASSERT_EQUAL_INT(0, ef.getEnvelopeLevel());

    g_analogValue = adcFromVoltage(2.20f);
    advanceMs(5);
    ef.update();
    TEST_ASSERT_TRUE(ef.getEnvelopeLevel() > 100);

    g_analogValue = adcFromVoltage(2.05f);
    advanceMs(5);
    ef.update();
    TEST_ASSERT_TRUE(ef.getEnvelopeLevel() > 0);

    g_analogValue = adcFromVoltage(1.70f);
    advanceMs(5);
    ef.update();
    TEST_ASSERT_EQUAL_INT(0, ef.getEnvelopeLevel());
}

void test_auto_baseline_converges() {
    EfHarnessGuard guard;
    PotentiometerManager pm(primaryMuxPins, secondaryMuxPins, potMuxAnalogPin);
    EnvelopeFollower ef(A0, &pm, 0);
    ef.setVref(1.65f);
    EnvelopeFollower::EfModeSettings settings = baseSettings(EnvelopeFollower::EFMode::Peak);
    settings.autoBaseline = true;
    settings.activityThreshold = 10;
    settings.baselineTauMs = 500;
    ef.setModeSettings(settings);

    g_analogValue = adcFromVoltage(1.75f);
    for (int i = 0; i < 50; ++i) {
        advanceMs(10);
        ef.update();
    }
    EnvelopeFollower::EfStats stats = ef.getStats();
    TEST_ASSERT_TRUE(stats.baseline > 0.05f);
    TEST_ASSERT_TRUE(stats.baseline < 0.2f);
}

void test_auto_gain_targets_level() {
    EfHarnessGuard guard;
    PotentiometerManager pm(primaryMuxPins, secondaryMuxPins, potMuxAnalogPin);
    EnvelopeFollower ef(A0, &pm, 0);
    ef.setVref(1.65f);
    EnvelopeFollower::EfModeSettings settings = baseSettings(EnvelopeFollower::EFMode::Peak);
    settings.autoGain = true;
    settings.autoBaseline = false;
    settings.gainTarget = 102;
    settings.gainTauMs = 500;
    settings.activityThreshold = 5;
    ef.setModeSettings(settings);

    g_analogValue = adcFromVoltage(1.90f);
    for (int i = 0; i < 80; ++i) {
        advanceMs(10);
        ef.update();
    }
    int level = ef.getEnvelopeLevel();
    TEST_ASSERT_TRUE(level > 70);
    TEST_ASSERT_TRUE(level < 120);
}

void test_stats_report_mode_and_value() {
    EfHarnessGuard guard;
    PotentiometerManager pm(primaryMuxPins, secondaryMuxPins, potMuxAnalogPin);
    EnvelopeFollower ef(A0, &pm, 0);
    ef.setVref(1.65f);
    ef.setModeSettings(baseSettings(EnvelopeFollower::EFMode::Follower));

    g_analogValue = adcFromVoltage(2.0f);
    advanceMs(5);
    ef.update();

    EnvelopeFollower::EfStats stats = ef.getStats();
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(EnvelopeFollower::EFMode::Follower),
                            static_cast<uint8_t>(stats.mode));
    TEST_ASSERT_TRUE(stats.value >= 0);
    TEST_ASSERT_TRUE(stats.gain > 0.0f);
}
