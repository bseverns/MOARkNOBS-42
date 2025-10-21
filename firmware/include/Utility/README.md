# Utility

Welcome to the grab‑bag of tricks that stops this synth from eating itself.
[Utility.h](../Utility.h) holds the declarations; this page is the street map.

## What it’s for

- **Value mapping** – warp raw numbers into whatever range you need.
  `mapToMidiValue()`, `mapToRange()`, `scale()`, and the exponential cousin all live here.
- **Tiny task schedulers** – three global punks: `schedulerHigh`, `schedulerMid`, and `schedulerLow`.
  They run cooperatively, so keep callbacks short and sweet.
- **Debounce helpers** – calm the switch chatter before it hits your logic.
- **EEPROM utilities** – read, write, and nuke bytes or words without touching the hardware registers directly.

## Quick hits

### Schedule a task
```cpp
#include "Utility.h"

void blink() {
  digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  // fire every 500 ms, forever
  Utility::schedulerLow.addTask(blink, 500, true);
}

void loop() {
  // must be called often or nothing runs
  Utility::schedulerLow.update();
}
```

### Map an analog read to MIDI 0‑127
```cpp
#include "Utility.h"
#include "Hardware/IO.h"

int raw = hardware::readAnalog(A0);
uint8_t midiValue = Utility::mapToMidiValue(raw);
// midiValue now rides 0‑127
```

## Gotchas

- `now()` comes from [`TimeUtils.h`](../TimeUtils.h) and is a weak wrapper around `millis()`.
  Tests can override it, so don’t stash its pointer expecting immortality.
- Tasks are collected first, then executed. If a callback adds or deletes tasks you’ll confuse the queue—schedule meta-work for the next tick instead.
- These schedulers don’t preempt; hog the CPU and you block the parade.

If you’re hunting the full API, crack open [Utility.h](../Utility.h).
