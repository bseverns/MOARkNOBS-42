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
        if (input.lfoActive[lfoIndex]) {
            value += static_cast<int>(input.lfoValue[lfoIndex]) - 64;
            value = constrain(value, 0, 127);
        }
    }
    return static_cast<uint8_t>(value);
}
