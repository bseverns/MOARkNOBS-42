# EnvelopeFollower

Sniffs audio or CV, shapes it, and hurls MIDI-friendly levels back.

## Key Methods

- `update()` – sample the pin and cook the envelope.
- `applyToCC(potIndex, value)` – mash a CC with the current level.
- `setFilterType(type)` – pick your flavor of chaos.
- `calibrate()` – sniff `VREF_ADC_PIN`, zero the noise floor, stash offsets.
  The baseline sticks in EEPROM so the follower wakes up ready to groove.
- `setVref(volts)` – if the auto-measured midpoint bugs you, slap in your own.

## Typical Use

```cpp
#include "EnvelopeFollower.h"

EnvelopeFollower env(A0, &pots);
env.setFilterType(EnvelopeFollower::LOWPASS);
env.setModulationTarget(10);
env.calibrate();       // learn & persist the baseline and voltage ref
env.toggleActive(true);

void loop() {
  env.update();
}
```

### Smoothing tweaks

`setOversampleCount(n)` and `setSmoothingAlpha(f)` let you trade jitter for
latency. Crank the sample count for cleaner reads, bump the alpha for snappier
response. The defaults (4 samples, 0.2f) play nice with most rigs.

### Calibration persistence

Baseline offsets survive reboots thanks to `ConfigManager`. The rig rehydrates
them at startup, but if you hold the first control button during power‑up it'll
recalibrate and stash fresh values. Punk enough?

Scope its internals in [EnvelopeFollower.h](../EnvelopeFollower.h).
