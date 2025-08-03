# Arpeggiator

Clock-synced riff machine that rips through a slot's note stack like it's late for soundcheck.

## Key Methods

- `start(slotIdx)` – point it at a slot and let the notes fly.
- `setLength(ms)` – tell it how fast to spit notes.
- `update(midi, cfg, pots)` – call every loop so it keeps drumming.

## Typical Use

```cpp
#include "Arpeggiator.h"

Arpeggiator arp;
arp.setLength(120); // 120 ms between hits
arp.start(0);       // chew on slot 0

void loop() {
  arp.update(midi, cfg, pots);
}
```

See the guts in [Arpeggiator.h](../Arpeggiator.h).
