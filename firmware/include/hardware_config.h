#pragma once

#include "Globals.h"

// Optional build-local scheduler tuning. This hook runs during setup(), after
// hardware managers have captured the board topology, so pins and LED/mux
// structure intentionally are not exposed here.
inline void applyHardwareRuntimeTuningOverrides(HardwareRuntimeTuning &tuning) {
    // Adjust task intervals here. Leaving this empty keeps the stock cadence.
}
