# LEDManager

Runs the 52-piece WS2812 light riot and keeps it barely under control.

## Key Methods

- `begin()` – initialize FastLED and blank the strip.
- `setPotValue(idx, value)` – mirror a slot's value on its halo.
- `update()` – push any dirty pixels out to the wire.

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
