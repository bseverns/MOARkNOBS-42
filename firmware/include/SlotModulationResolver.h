#ifndef SLOT_MODULATION_RESOLVER_H
#define SLOT_MODULATION_RESOLVER_H

#include <array>
#include <cstdint>

#include "MIDITypes.h"

// One control-rate snapshot of every source that can bend a slot value.
// Transport emission deliberately lives outside this value-only resolver.
struct SlotModulationInput {
    uint8_t baseline = 0;
    bool efActive = false;
    uint8_t efValue = 0;
    EfDestinationMode efMode = EfDestinationMode::AddClamp;
    std::array<bool, 2> lfoActive{};
    std::array<float, 2> lfoNormalized{}; // Unipolar oscillator value (0..1)
    std::array<float, 2> lfoSigned{};     // Bipolar oscillator value (-1..1)
    std::array<SlotLfoLane, 2> lfoLane{};
};

// Compose sources in the fixed order baseline -> EF/ARG -> LFO 1 -> LFO 2.
// AddClamp/Subtract consume normalized LFO values; Centered, Replace, and
// Scale consume signed values. Centered uses -64..+63 at full amount so the
// physical control remains the center of gravity without excessive clipping.
uint8_t resolveSlotModulation(const SlotModulationInput &input);

#endif // SLOT_MODULATION_RESOLVER_H
