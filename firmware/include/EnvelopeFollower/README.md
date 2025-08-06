# EnvelopeFollower

Part of the firmware `include` jungle. Hit the [parent README](../README.md) for how envelopes boss the rest.

Sniffs audio or CV, shapes it, and hurls MIDI-friendly levels back.

## Key Methods

- `update()` – sample the pin and cook the envelope.
- `applyToCC(potIndex, value)` – mash a CC with the current level.
- `setFilterType(type)` – pick your flavor of chaos.
- `calibrate()` – sniff `VREF_ADC_PIN`, zero the noise floor, stash offsets to EEPROM.

## Typical Use

```cpp
#include "EnvelopeFollower.h"

EnvelopeFollower env(A0, &pots, 0); // third arg tags it for EEPROM
env.setFilterType(EnvelopeFollower::LOWPASS);
env.setModulationTarget(10);
env.calibrate();       // learn the baseline, stash it in EEPROM
env.toggleActive(true);

void loop() {
  env.update();
}
```

### Smoothing tweaks

`setOversampleCount(n)` and `setSmoothingAlpha(f)` let you trade jitter for
latency. Crank the sample count for cleaner reads, bump the alpha for snappier
response. The defaults (4 samples, 0.2f) play nice with most rigs.

Scope its internals in [EnvelopeFollower.h](../EnvelopeFollower.h).
