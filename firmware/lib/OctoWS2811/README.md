# OctoWS2811 (Vendored)

This is a chopped-down, local copy of Paul Stoffregen's OctoWS2811 library. We only keep enough scaffolding around so FastLED's Teensy glue code can compile without reaching out to the internet.

The upstream Teensy core's `DMAChannel.h` trips `-Wdeprecated-copy` warnings, which we treat as build-busting errors. To keep the compiler happy without dropping our warning pedal to the floor, a wrapper `DMAChannel.h` lives here and `#include_next`s the real deal while muting the offending warning.

If you update the Teensy core or need more from OctoWS2811, crack open the official repo and sync what you need into this folder.
