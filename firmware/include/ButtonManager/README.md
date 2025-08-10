# ButtonManager

Part of the firmware `include` jungle. The [include README](../README.md) explains how button rage propagates; the [main firmware README](../../README.md) zooms out to the whole machine.

Scans the 7×6 button grid, smacks bounce in the teeth, and spits out events.

## Where it fits

ButtonManager rides the multiplexed button grid, feeds MIDIHandler and flashes clues through LEDManager. ConfigManager keeps the map of what each press actually means.

```
[Muxed buttons] --> ButtonManager --> MIDIHandler
                                \-> LEDManager
```

See the big picture in the [main firmware README](../../README.md).

## Key Methods

- `initButtons()` – wire up mux pins and ready the machines.
- `processButtons(ctx)` – poll the matrix and trigger actions.
- `isMuxButtonPressed(idx)` – peek a raw button for tests.

## Typical Use

```cpp
#include "ButtonManager.h"

ButtonManager buttons(hwConfig, CONTROL_PINS, &pots);

void setup() {
  buttons.initButtons();
}

void loop() {
  buttons.processButtons(ctx);
}
```

That `hwConfig` bundle wrangles mux pins, LED counts, and timing so the tests and firmware slam in sync.

Dig deeper in [ButtonManager.h](../ButtonManager.h).
