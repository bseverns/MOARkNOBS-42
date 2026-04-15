# LEDManager

Part of the firmware `include` jungle. Scope the [include README](../README.md) to see how the glow plugs in and the [main firmware README](../../README.md) for the megawatt overview.

Runs the 52-piece WS2812 light riot and keeps it barely under control.

![Main LED pool wiring](../../../docs/sketch/MainLEDPool.png)

## Where it fits

LEDManager mirrors pot moves and config quirks on a WS2812 strip; EnvelopeFollower can hijack it for visual throb.

```
[Pots/Config] --> LEDManager --> WS2812 strip
```

Catch the full scene in the [main firmware README](../../README.md).

## Key Methods

- `begin()` – initialize FastLED and blank the strip.
- `setPotValue(idx, value)` – mirror a slot's value on its halo.
- `setPixelColor(idx, color)` – paint one exact RGB color onto one LED.
- `update()` – push any dirty pixels out to the wire.

## Default State

Fresh out of construction, `LEDManager` is chilling in `LEDState::IDLE` with
`activeIndex` parked at `255` – a sentinel meaning "no LED is currently under
the spotlight."  It's a clean slate; nothing gets lit until you tell it to.

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

Tired of magic numbers? `HardwareConfig` (defined in [`Globals.h`](../Globals.h)) still
tracks the board's pins and timing knobs. `LEDManager` leans on it for odds and
ends like the status LED, but the strip's data pin is now a compile-time rebel:
`LED_DATA_PIN` in `platformio.ini` decides where the bits fly. Want to reroute
the glow? Change that flag and rebuild—runtime config won't save you.

Peer at the wiring in [LEDManager.h](../LEDManager.h).
