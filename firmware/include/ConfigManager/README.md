# ConfigManager

Part of the firmware `include` jungle. Read the [include README](../README.md) to see how configs steer the beast, and the [main firmware README](../../README.md) for the broader manifesto.

Keeps the controller's brain in EEPROM so your madness survives power cycles.

## Where it fits

ConfigManager brokers EEPROM secrets for PotentiometerManager, EnvelopeFollower, and LEDManager so everyone plays the right tune at boot.

```
EEPROM <--> ConfigManager <--> Pots/Envelope/LEDs
```

More lore in the [main firmware README](../../README.md).

## Key Methods

- `begin(potChannels)` – load saved settings at boot.
- `getSlotType(idx)` – figure out what a slot sends, now including NRPN and SysEx weirdness.
- `saveConfiguration()` – write everything back to flash.
- `loadEnvelopeSettings(map, envs)` – patch in EF routing *and* baselines; returns false if any follower still needs to find its feet.

## Typical Use

```cpp
#include "ConfigManager.h"

ConfigManager cfg(NUM_POTS, NUM_BUTTONS);
std::vector<uint8_t> potCh(NUM_POTS);
cfg.begin(potCh);

cfg.setSlotType(0, MIDI_CC);
cfg.saveConfiguration();
```

The full saga lives in [ConfigManager.h](../ConfigManager.h).
