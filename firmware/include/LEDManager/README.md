# LEDManager

Runs the 52-piece WS2812 light riot and keeps it barely under control.

## Key Bits

- Constructor grabs `cfg.ledPin` and tells FastLED where the strip lives.
- `setPotValue(idx, value)` – mirror a slot's value on its halo.
- `update()` – push any dirty pixels out to the wire.

## Typical Use

```cpp
#include "Globals.h"      // gives you hwConfig
#include "LEDManager.h"

LEDManager leds(hwConfig); // runtime pin comes from cfg.ledPin

void loop() {
  leds.setPotValue(0, 127);
  leds.update();
}
```

Peer at the wiring in [LEDManager.h](../LEDManager.h).
