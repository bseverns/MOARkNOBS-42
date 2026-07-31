#include "SlotModulationResolver.h"

#include <Arduino.h>
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
} // namespace

uint8_t resolveSlotModulation(const SlotModulationInput &input) {
    int value = input.baseline;
    if (input.efActive) {
        value = applyDestinationMode(value, input.efValue, input.efMode);
    }
    value = constrain(value, 0, 127);

    for (size_t lfoIndex = 0; lfoIndex < input.lfoActive.size(); ++lfoIndex) {
        if (!input.lfoActive[lfoIndex]) continue;
        const SlotLfoLane lane = sanitizeSlotLfoLane(input.lfoLane[lfoIndex]);
        const float signedLfo = constrain(input.lfoValue[lfoIndex], -1.0f, 1.0f);
        const float amount = static_cast<float>(lane.amount) / 100.0f;
        const int offset = static_cast<int>(std::lround(signedLfo * amount * 127.0f));
        switch (lane.mode()) {
        case ModCombineMode::AddClamp:
        case ModCombineMode::Centered:
            value += offset;
            break;
        case ModCombineMode::Subtract:
            value -= offset;
            break;
        case ModCombineMode::Replace:
            value = static_cast<int>(std::lround(64.0f + signedLfo * amount * 63.0f));
            break;
        case ModCombineMode::Scale:
            value = static_cast<int>(std::lround(
                static_cast<float>(value) * (1.0f + signedLfo * amount)));
            break;
        }
        value = constrain(value, 0, 127);
    }
    return static_cast<uint8_t>(value);
}
