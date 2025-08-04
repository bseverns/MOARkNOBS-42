# ConfigManager

Part of the firmware `include` jungle. Read the [parent README](../README.md) to see how configs steer the beast.

Keeps the controller's brain in EEPROM so your madness survives power cycles.

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
