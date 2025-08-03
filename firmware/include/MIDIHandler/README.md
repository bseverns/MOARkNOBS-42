# MIDIHandler

USB, DIN, whatever—this thing speaks MIDI like it's 1983.

## Key Methods

- `begin()` – open both MIDI pipes.
- `sendControlChange(cc, value, channel)` – fire a CC.
- `processIncomingMIDI()` – keep an ear on incoming bytes.

## Typical Use

```cpp
#include "MIDIHandler.h"

MIDIHandler midi;

void setup() {
  midi.begin();
  midi.sendControlChange(74, 99, 1);
}

void loop() {
  midi.processIncomingMIDI();
}
```

Scope the I/O in [MIDIHandler.h](../MIDIHandler.h).
