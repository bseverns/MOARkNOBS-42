#include "SlotModulationResolver.h"

#include <algorithm>
#include <cmath>

namespace {
int applyDestinationMode(int baseline, uint8_t contribution, EfDestinationMode mode) {
    switch (mode) {
    case EfDestinationMode::AddClamp:
        return baseline + contribution;
    case EfDestinationMode::Subtract:
        return baseline - contribution;
    case EfDestinationMode::Replace:
        return contribution;
    case EfDestinationMode::Scale:
        return static_cast<int>(std::lround(
            (static_cast<float>(baseline) * static_cast<float>(contribution)) / 127.0f));
    case EfDestinationMode::Centered:
        return baseline + static_cast<int>(contribution) - 64;
    }
    return baseline;
}

int unipolarContribution(float normalizedLfo, float amount) {
    return static_cast<int>(std::lround(std::clamp(normalizedLfo, 0.0f, 1.0f) * amount *
                                        127.0f));
}

int centeredContribution(float signedLfo, float amount) {
    const float modulation = std::clamp(signedLfo, -1.0f, 1.0f) * amount;
    const float range = modulation < 0.0f ? 64.0f : 63.0f;
    return static_cast<int>(std::lround(modulation * range));
}
} // namespace

float signedSlotLfoFromMidiValue(uint8_t value) {
    const uint8_t midiValue = std::min<uint8_t>(value, 127);
    return midiValue < 64 ? (static_cast<float>(midiValue) - 64.0f) / 64.0f
                          : (static_cast<float>(midiValue) - 64.0f) / 63.0f;
}

uint8_t resolveSlotModulation(const SlotModulationInput &input) {
    int value = input.baseline;
    if (input.efActive) {
        value = applyDestinationMode(value, input.efValue, input.efMode);
    }
    value = std::clamp(value, 0, 127);

    for (size_t lfoIndex = 0; lfoIndex < input.lfoActive.size(); ++lfoIndex) {
        if (!input.lfoActive[lfoIndex]) continue;
        const SlotLfoLane lane = sanitizeSlotLfoLane(input.lfoLane[lfoIndex]);
        const float signedLfo = std::clamp(input.lfoSigned[lfoIndex], -1.0f, 1.0f);
        const float amount = static_cast<float>(lane.amount) / 100.0f;
        switch (lane.mode()) {
        case ModCombineMode::AddClamp:
            value += unipolarContribution(input.lfoNormalized[lfoIndex], amount);
            break;
        case ModCombineMode::Subtract:
            value -= unipolarContribution(input.lfoNormalized[lfoIndex], amount);
            break;
        case ModCombineMode::Centered:
            value += centeredContribution(signedLfo, amount);
            break;
        case ModCombineMode::Replace:
            value = 64 + centeredContribution(signedLfo, amount);
            break;
        case ModCombineMode::Scale:
            value = static_cast<int>(std::lround(
                static_cast<float>(value) * (1.0f + signedLfo * amount)));
            break;
        }
        value = std::clamp(value, 0, 127);
    }
    return static_cast<uint8_t>(value);
}
