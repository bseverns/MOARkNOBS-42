# LEDManager

Runs the 52-piece WS2812 light riot and keeps it barely under control.

## Key Methods

- `begin()` – initialize FastLED and blank the strip.
- `setPotValue(idx, value)` – mirror a slot's value on its halo.
- `update()` – push any dirty pixels out to the wire.

## Default State

Fresh out of construction, `LEDManager` is chilling in `LEDState::IDLE` with
`activeIndex` parked at `255` – a sentinel meaning "no LED is currently under
the spotlight."  It's a clean slate; nothing gets lit until you tell it to.

## Typical Use

```cpp
#include "LEDManager.h"

LEDManager leds(NUM_LEDS);

void setup() {
  leds.begin();
}

void loop() {
  leds.setPotValue(0, 127);
  leds.update();
}
```

Peer at the wiring in [LEDManager.h](../LEDManager.h).
