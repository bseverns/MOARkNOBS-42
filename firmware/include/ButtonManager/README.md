# ButtonManager

Scans the 7×6 button grid, smacks bounce in the teeth, and spits out events.

## Key Methods

- `initButtons()` – wire up mux pins and ready the machines.
- `processButtons(ctx)` – poll the matrix and trigger actions.
- `isMuxButtonPressed(idx)` – peek a raw button for tests.

## Typical Use

```cpp
#include "ButtonManager.h"

ButtonManager buttons(MUXR_PINS, MUXC_PINS, buttonMuxAnalogPin, CONTROL_PINS, &pots);

void setup() {
  buttons.initButtons();
}

void loop() {
  buttons.processButtons(ctx);
}
```

Dig deeper in [ButtonManager.h](../ButtonManager.h).
