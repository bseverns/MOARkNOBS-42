# EnvelopeFollower

Sniffs audio or CV, shapes it, and hurls MIDI-friendly levels back.

## Key Methods

- `update()` – sample the pin and cook the envelope.
- `applyToCC(potIndex, value)` – mash a CC with the current level.
- `setFilterType(type)` – pick your flavor of chaos.

## Typical Use

```cpp
#include "EnvelopeFollower.h"

EnvelopeFollower env(A0, &pots);
env.setFilterType(EnvelopeFollower::LOWPASS);
env.setModulationTarget(10);
env.toggleActive(true);

void loop() {
  env.update();
}
```

Scope its internals in [EnvelopeFollower.h](../EnvelopeFollower.h).
