#pragma once
// Sneak this wrapper in before the real Teensy DMAChannel header.
// It pulls in the core header but muzzles the deprecated-copy whine
// so our -Werror builds don't light up like a pinball machine.

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-copy"
#endif

#include_next <DMAChannel.h>

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

