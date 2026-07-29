#ifndef EF_FILTER_CONTROL_H
#define EF_FILTER_CONTROL_H

#include <stdint.h>

// Persisted/configured EF "frequency" values are a legacy shaping-control
// scale. They were historically evaluated against a fictional 44.1 kHz
// sample rate, so they are not physical cutoff frequencies. Translate that
// control ratio onto the real control-rate cadence before configuring a DSP
// primitive. This preserves the established musical response while making
// the timing model explicit and testable.
constexpr float EF_FILTER_REFERENCE_RATE_HZ = 44100.0f;
constexpr uint8_t EF_FOLLOWER_SCHEDULER_PERIOD_MS = 2;
constexpr uint8_t EF_FOLLOWERS_PER_PASS = 1;

inline constexpr float efControlRateHz(float schedulerPeriodMs, float followersPerPass,
                                       float followerCount) {
    return (1000.0f * followersPerPass) / (schedulerPeriodMs * followerCount);
}

inline constexpr float efControlToCutoffHz(float controlValue, float controlRateHz) {
    return controlValue * controlRateHz / EF_FILTER_REFERENCE_RATE_HZ;
}

#endif // EF_FILTER_CONTROL_H
