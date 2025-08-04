# PotentiometerManager

Part of the firmware `include` jungle. See the [parent README](../README.md) to map the territory.

Reads the three real pots through a mess of muxes and smooths their jittery souls.

## Key Methods

- `setMidiCallback(cb)` – tell it how to blast MIDI when a pot moves.
- `processPots(leds, envelopes)` – scan, smooth, and dispatch.
- `loadFromEEPROM()` – pull channel/CC maps from storage.

## Typical Use

```cpp
#include "PotentiometerManager.h"

PotentiometerManager pots(MUXR_PINS, MUXC_PINS, potMuxAnalogPin);

void setup() {
  pots.setMidiCallback(sendMidi);
  pots.loadFromEEPROM();
}

void loop() {
  pots.processPots(leds, envelopes);
}
```

Gory details in [PotentiometerManager.h](../PotentiometerManager.h).
