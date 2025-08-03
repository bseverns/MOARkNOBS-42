# DisplayManager

Talks to the SSD1306 OLED and isn't afraid to shout.

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
