#pragma once

#include "Globals.h"

// Hack this file to bend the rig to your will.
// By default it's a no-op so the stock settings scream on.
inline void applyHardwareConfigOverrides(HardwareConfig &cfg) {
    // Drop your custom pin swaps or timing tweaks here.
    // Leaving this empty means the defaults in Globals.cpp reign supreme.
}
