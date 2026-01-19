#ifndef EF_VOICE_H
#define EF_VOICE_H

#include <Arduino.h>
#include <cmath>

#include "BiquadFilter.h"
#include "EnvelopeFollower.h"
#include "EfSettingsUtils.h"
#include "Globals.h"
#include "PerlinNoise.h"
#include "TimeUtils.h"

inline BiquadFilter::FilterType toBiquadType(EnvelopeFollower::FilterType type) {
    switch (type) {
    case EnvelopeFollower::LOWPASS:
        return BiquadFilter::LOWPASS;
    case EnvelopeFollower::HIGHPASS:
        return BiquadFilter::HIGHPASS;
    case EnvelopeFollower::BANDPASS:
        return BiquadFilter::BANDPASS;
    default:
        return BiquadFilter::LOWPASS;
    }
}

inline float jitterRateFromSmoothness(float smoothness) {
    float clamped = constrain(smoothness, 0.0f, 1.0f);
    return 0.05f + (1.0f - clamped) * 1.95f;
}

inline float jitterDepth() { return constrain(g_jitterSettings.depth, 0.0f, 1.0f); }

struct EfVoice {
    uint8_t followerIndex = 0xFF; //!< Physical follower index we mirror
    EnvelopeFollower::FilterType filterType = EnvelopeFollower::LINEAR;
    float frequency = 1000.0f;
    float q = 0.707f;
    bool filterDirty = true;
    bool hasFollower = false;
    uint8_t lastLevel = 0;
    bool hasLevel = false;
    BiquadFilter filter;

    void resetFollower() {
        followerIndex = 0xFF;
        hasFollower = false;
        hasLevel = false;
        filterDirty = true;
    }

    void assignFollower(int index) {
        if (index < 0) {
            resetFollower();
            return;
        }
        uint8_t next = static_cast<uint8_t>(index);
        if (!hasFollower || followerIndex != next) {
            followerIndex = next;
            hasFollower = true;
            hasLevel = false;
            filterDirty = true;
        }
    }

    void syncSettings(const EfSettings &settings) {
        EnvelopeFollower::FilterType nextType = decodeFilterType(settings.filterType);
        float nextFreq = settings.frequency;
        float nextQ = settings.q;
        if (!hasFollower) {
            filterType = nextType;
            frequency = nextFreq;
            q = nextQ;
            filterDirty = true;
            return;
        }
        if (filterType != nextType || frequency != nextFreq || q != nextQ) {
            filterType = nextType;
            frequency = nextFreq;
            q = nextQ;
            filterDirty = true;
        }
    }

    uint8_t render(int rawLevel) {
        if (!hasFollower) {
            hasLevel = false;
            lastLevel = 0;
            return 0;
        }

        int level = constrain(rawLevel, 0, 127);
        uint8_t shaped = 0;

        switch (filterType) {
        case EnvelopeFollower::LOWPASS:
        case EnvelopeFollower::HIGHPASS:
        case EnvelopeFollower::BANDPASS: {
            if (filterDirty) {
                filter.configure(toBiquadType(filterType), frequency, 44100.0f, q);
                filterDirty = false;
            }
            float processed = filter.process(static_cast<float>(level));
            shaped = static_cast<uint8_t>(constrain(static_cast<int>(roundf(processed)), 0, 127));
            break;
        }

        case EnvelopeFollower::OPPOSITE_LINEAR: {
            float scaled = static_cast<float>(level) * (frequency / 1000.0f);
            shaped = static_cast<uint8_t>(constrain(127 - static_cast<int>(scaled), 0, 127));
            break;
        }

        case EnvelopeFollower::EXPONENTIAL: {
            float ratio = level / 127.0f;
            float curved = powf(ratio, q) * (frequency / 1000.0f) * 127.0f;
            shaped = static_cast<uint8_t>(constrain(static_cast<int>(roundf(curved)), 0, 127));
            break;
        }

        case EnvelopeFollower::RANDOM: {
            int probability = map(static_cast<int>(frequency), 20, 5000, 0, 100);
            probability = constrain(probability, 0, 100);
            if (random(0, 100) < probability) {
                int range = map(static_cast<int>(q * 100.0f), 50, 400, 1, 64);
                float depth = jitterDepth();
                if (depth <= 0.0f) {
                    shaped = static_cast<uint8_t>(level);
                    break;
                }
                float rate = jitterRateFromSmoothness(g_jitterSettings.smoothness);
                float t = (static_cast<float>(now()) * 0.001f * rate) +
                          (static_cast<float>(followerIndex) * 17.23f);
                float n = perlinNoise1D(t);
                int swing = constrain(range, 1, 64);
                int jitter = static_cast<int>(roundf(n * static_cast<float>(swing) * depth));
                shaped = static_cast<uint8_t>(constrain(level + jitter, 0, 127));
            } else {
                shaped = static_cast<uint8_t>(level);
            }
            break;
        }

        case EnvelopeFollower::LINEAR:
        default: {
            float scaled = static_cast<float>(level) * (frequency / 1000.0f);
            shaped = static_cast<uint8_t>(constrain(static_cast<int>(roundf(scaled)), 0, 127));
            break;
        }
        }

        lastLevel = shaped;
        hasLevel = true;
        return shaped;
    }

    uint8_t latestLevel() const { return hasLevel ? lastLevel : 0; }
    bool hasRendered() const { return hasLevel; }
};

#endif // EF_VOICE_H
