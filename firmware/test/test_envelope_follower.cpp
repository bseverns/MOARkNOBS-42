#include "unity_config.h"
#include <unity.h>

#include <cmath>

#include "EnvelopeFollower.h"
#include "Hardware/IO.h"
#include "PotentiometerManager.h"
#include "Globals.h"
#include "TimeStub.h"

namespace {
constexpr float kTwoPi = 6.283185307179586f;

int g_testAnalog = 0;

int analogProvider(uint8_t) { return static_cast<int>(g_testAnalog); }

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

struct EfHarnessGuard {
    EfHarnessGuard() {
        hardware::setAnalogReadProvider(analogProvider);
        g_testAnalog = 0;
        g_fakeNowMs = 0;
    }
    ~EfHarnessGuard() { hardware::resetAnalogReadProvider(); }
};
} // namespace

void test_envelope_stats_sine_trace() {
    EfHarnessGuard guard;
    PotentiometerManager pm(primaryMuxPins, secondaryMuxPins, potMuxAnalogPin);
    EnvelopeFollower ef(A0, &pm, 0);
    ef.setVref(1.65f);
    ef.setModeSettings(baseSettings(EnvelopeFollower::EFMode::Peak));

    const float amplitude = powf(10.0f, -10.0f / 20.0f) * sqrtf(2.0f);
    for (int i = 0; i < 24; ++i) {
        float angle = (static_cast<float>(i) / 24.0f) * kTwoPi;
        float voltage = 1.65f + sinf(angle) * amplitude * 0.5f;
        g_testAnalog = adcFromVoltage(voltage);
        advanceMs(1);
        ef.update();
    }

    EnvelopeFollower::EfStats stats = ef.getStats();
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(EnvelopeFollower::EFMode::Peak), stats.mode);
    TEST_ASSERT_TRUE(stats.value >= 0.0f);
    TEST_ASSERT_TRUE(stats.value <= 127.0f);
    TEST_ASSERT_TRUE(stats.gain > 0.0f);
}

void test_envelope_stats_step_trace() {
    EfHarnessGuard guard;
    PotentiometerManager pm(primaryMuxPins, secondaryMuxPins, potMuxAnalogPin);
    EnvelopeFollower ef(A0, &pm, 0);
    ef.setVref(1.65f);
    ef.setModeSettings(baseSettings(EnvelopeFollower::EFMode::Peak));

    g_testAnalog = adcFromVoltage(0.1f);
    for (int i = 0; i < 8; ++i) {
        advanceMs(1);
        ef.update();
    }

    g_testAnalog = adcFromVoltage(3.3f);
    for (int i = 0; i < 8; ++i) {
        advanceMs(1);
        ef.update();
    }

    EnvelopeFollower::EfStats stats = ef.getStats();
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(EnvelopeFollower::EFMode::Peak), stats.mode);
    TEST_ASSERT_TRUE(stats.value > 60.0f);
    TEST_ASSERT_TRUE(stats.baseline >= 0.0f);
}

void test_envelope_stats_idle_noise() {
    EfHarnessGuard guard;
    PotentiometerManager pm(primaryMuxPins, secondaryMuxPins, potMuxAnalogPin);
    EnvelopeFollower ef(A0, &pm, 0);
    ef.setVref(1.65f);
    EnvelopeFollower::EfModeSettings settings = baseSettings(EnvelopeFollower::EFMode::Peak);
    settings.autoBaseline = true;
    settings.activityThreshold = 8;
    settings.baselineTauMs = 500;
    ef.setModeSettings(settings);

    for (int i = 0; i < 40; ++i) {
        float jitter = (static_cast<float>(i % 3) - 1.0f) * 0.02f;
        float voltage = 1.65f + jitter;
        g_testAnalog = adcFromVoltage(voltage);
        advanceMs(1);
        ef.update();
    }

    EnvelopeFollower::EfStats stats = ef.getStats();
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(EnvelopeFollower::EFMode::Peak), stats.mode);
    TEST_ASSERT_TRUE(stats.baseline >= 0.0f);
    TEST_ASSERT_TRUE(stats.gain >= 0.0f);
}
