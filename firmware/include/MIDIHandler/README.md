# MIDIHandler

USB, DIN, whatever—this thing speaks MIDI like it's 1983 and even keeps time for the slackers.

## Key Methods

- `begin()` – open both MIDI pipes.
- `sendControlChange(cc, value, channel)` – fire a CC.
- `sendClock()` – spit out a raw 0xF8 when you want to be the metronome.
- `processIncomingMIDI()` – keep an ear on incoming bytes **and** spew MIDI clock when `g_tappedBPM` says so.

Clock out defers to any incoming tempo; if the outside world goes dark for `CLOCK_TIMEOUT_MS`
the tapped BPM drags the beat back to life. Smash Control #1 + #2 to toggle that clock stream
whenever you feel like it.

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
