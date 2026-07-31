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
    std::array<uint8_t, 2> lfoValue{{64, 64}};
};

// Compose sources in the fixed order baseline -> EF/ARG -> LFO 1 -> LFO 2.
// Slot LFO route values are interpreted as centered offsets so the physical
// control remains the center of gravity; 64 is neutral.
uint8_t resolveSlotModulation(const SlotModulationInput &input);

#endif // SLOT_MODULATION_RESOLVER_H
