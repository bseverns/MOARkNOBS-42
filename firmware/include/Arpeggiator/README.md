# Arpeggiator

Part of the firmware `include` jungle. The [parent README](../README.md) shows how it locks to the rest of the circus.

Clock-synced riff machine that rips through a slot's note stack like it's late for soundcheck.

## Key Methods

- `start(slotIdx)` – point it at a slot and let the notes fly.
- `setLength(ms)` – tell it how fast to spit notes.
- `setPatternLength(steps)` – define how many steps and semitones the loop spans.
- `update(midi, cfg, pots)` – call every loop so it keeps drumming.

`noteOffset(shape, step, patternLen)` handles the pitch math. `patternLen`
(2–16) sets how many semitone rungs the arp climbs before wrapping. `noteOffset`
then spits the actual jump each tick: `UP` counts up, `DOWN` walks back to zero,
`UPDOWN` mirrors the climb, and `RANDOM` lobs a number somewhere in range.

## Typical Use

```cpp
#include "Arpeggiator.h"

Arpeggiator arp;
arp.setLength(120); // 120 ms between hits
arp.setPatternLength(4); // four-step pattern, offsets 0-3
arp.start(0);       // chew on slot 0

void loop() {
  arp.update(midi, cfg, pots);
}
```

See the guts in [Arpeggiator.h](../Arpeggiator.h).
