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

// Ordered resolver trace used by telemetry and host explanations. Each delta
// is measured after that lane's clamp/replace/scale operation, so baseline +
// efDelta + lfoDelta[0] + lfoDelta[1] always equals finalValue.
struct SlotModulationResult {
    uint8_t baseline = 0;
    int16_t efDelta = 0;
    std::array<int16_t, 2> lfoDelta{};
    bool efApplied = false;
    std::array<bool, 2> lfoApplied{};
    uint8_t finalValue = 0;
};

// Compose sources in the fixed order baseline -> EF/ARG -> LFO 1 -> LFO 2.
// AddClamp/Subtract consume normalized LFO values; Centered, Replace, and
// Scale consume signed values. Centered uses -64..+63 at full amount so the
// physical control remains the center of gravity without excessive clipping.
uint8_t resolveSlotModulation(const SlotModulationInput &input);
SlotModulationResult resolveSlotModulationWithContributions(const SlotModulationInput &input);

// Convert a complete 7-bit legacy SlotValue route output back into the
// asymmetric bipolar range consumed by Replace. This preserves 0, 64, and 127
// exactly and keeps legacy output independent of the physical pot baseline.
float signedSlotLfoFromMidiValue(uint8_t value);

#endif // SLOT_MODULATION_RESOLVER_H
