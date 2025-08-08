# DisplayManager

Part of the firmware `include` jungle. Check the [include README](../README.md) if you get lost in pixels, and the [main firmware README](../../README.md) for the grand tour.

Talks to the SSD1306 OLED and isn't afraid to shout.

## Where it fits

DisplayManager takes button-driven context and splashes it to the SSD1306 over I2C. ConfigManager tells it what to show.

```
[Button ctx] --> DisplayManager --> SSD1306
```

More backstory in the [main firmware README](../../README.md).

## Key Methods

- `begin()` – fire up the display hardware.
- `showText(line1, line2, line3)` – scribble three lines and bail.
- `updateFromContext(ctx)` – let button events drive the UI.

## Typical Use

```cpp
#include "DisplayManager.h"

DisplayManager screen(0x3C, 128, 64);

void setup() {
  screen.begin();
  screen.showText("MN42", "ready");
}

void loop() {
  screen.updateFromContext(ctx);
}
```

For more pixels see [DisplayManager.h](../DisplayManager.h).
