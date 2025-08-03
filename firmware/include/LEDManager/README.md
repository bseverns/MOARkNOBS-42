# LEDManager

Runs the 52-piece WS2812 light riot and keeps it barely under control.

## Key Methods

- `begin()` – initialize FastLED and blank the strip.
- `setPotValue(idx, value)` – mirror a slot's value on its halo.
- `update()` – push any dirty pixels out to the wire.

## Typical Use

```cpp
#include "Globals.h"
#include "LEDManager.h"

// `hwConfig` gets filled at boot with pin numbers and LED counts.
LEDManager leds(hwConfig);

void setup() {
  leds.begin();
}

void loop() {
  leds.setPotValue(0, 127);
  leds.update();
}
```

## HardwareConfig: the Puppet Master

Tired of magic numbers? `HardwareConfig` (defined in [`Globals.h`](../Globals.h)) is
the project's central ledger for every pin, LED count, and timing knob.
`LEDManager` siphons the LED data pin and strip length straight from that
struct, so you can reroute wires or swap strips without rewriting code.
Want different pins? Override `hwConfig` with a `hardware_config.h` or even a
tiny `hardware_config.json` and the lights obey.

Peer at the wiring in [LEDManager.h](../LEDManager.h).
