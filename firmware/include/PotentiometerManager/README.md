# PotentiometerManager

Part of the firmware `include` jungle. See the [include README](../README.md) to map the territory and the [main firmware README](../../README.md) for the whole rig.

Reads the three real pots through a mess of muxes and smooths their jittery souls.

## Where it fits

PotentiometerManager sucks analog off the muxes, tells MIDIHandler what moved, and gives LEDManager and ConfigManager the numbers they need.

```
[Pots via mux] --> PotentiometerManager --> MIDIHandler
                                    \-> LEDManager
                                    \-> ConfigManager
```

See the bigger picture in the [main firmware README](../../README.md).

## Key Methods

- `setMidiCallback(cb)` – tell it how to blast MIDI when a pot moves. The callback is
  `void(uint8_t ccNumber, uint8_t midiValue, uint16_t smoothedAdc, uint8_t slotIndex)`;
  snag the MIDI channel from your slot table, not from the callback args.
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
