# ConfigManager

Keeps the controller's brain in EEPROM so your madness survives power cycles.

## Key Methods

- `begin(potChannels)` – load saved settings at boot.
- `getSlotType(idx)` – figure out what a slot sends, now including NRPN and SysEx weirdness.
- `saveConfiguration()` – write everything back to flash.
- `loadEnvelopeCalibrations(envelopes)` – hydrate envelope followers with their
  saved baselines. Call `saveEnvelopeCalibration(idx, b)` after calibrating to
  stash new offsets.

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
