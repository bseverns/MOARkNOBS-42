# Arpeggiator

Part of the firmware `include` jungle. The [include README](../README.md) shows how it locks to the rest of the circus; the [main firmware README](../../README.md) explains why the band even exists.

Clock-synced riff machine that rips through a slot's note stack like it's late for soundcheck.
Now it can yank its root note from wherever you tell it—slot memory, envelope follower, or some mystery source you cooked up.

## Where it fits

Arpeggiator chews on slot memory or an EnvelopeFollower and spits the notes through MIDIHandler.

```
[Slots/env] --> Arpeggiator --> MIDIHandler
```

Get the bird's-eye in the [main firmware README](../../README.md).

## Key Methods

- `start(slotIdx)` – point it at a slot and let the notes fly.
- `setLength(ticks)` – how many MIDI clock ticks to wait between hits (max 24).
- `setPatternLength(steps)` – define how many steps and semitones the loop spans.
- `setBaseNoteSource(src)` – choose who owns the root (`Slot` or `External`).
- `setBaseNote(note)` / `setBaseNoteCallback(fn)` – shove in a fresh base note or a function that returns one.
- `update(midi, cfg, pots)` – call every loop so it keeps drumming.

`noteOffset(shape, step, patternLen)` handles the pitch math. `patternLen`
(2–16) sets how many semitone rungs the arp climbs before wrapping. `noteOffset`
then spits the actual jump each tick: `UP` counts up, `DOWN` walks back to zero,
`UPDOWN` mirrors the climb, and `RANDOM` lobs a number somewhere in range.

## Arp Settings

| Setting | Range / Options | What it does |
| ------- | --------------- | ------------ |
| Length | 1–24 ticks | MIDI clock ticks between notes |
| Pattern Length | 2–16 steps | Semitone span before the loop repeats |
| Mode | UP, DOWN, UPDOWN, RANDOM | Direction for `noteOffset` |
| Base Note Source | Slot, External | Where the root comes from |
| Base Note / Callback | 0–127 or func | Force a root note or supply a generator |

Dial these in and the arp will march (or stumble) exactly how you tell it.

## Typical Use

```cpp
#include "Arpeggiator.h"

Arpeggiator arp;
arp.setLength(12);       // fire every 12 clock ticks
arp.setPatternLength(4); // four-step pattern, offsets 0-3
arp.start(0);       // chew on slot 0

void loop() {
  arp.update(midi, cfg, pots);
}
```

See the guts in [Arpeggiator.h](../Arpeggiator.h).

## Base Notes that Don't Rot

`_baseNote` isn't chained to one sad MIDI number. Feed it whatever
`EnvelopeFollower` or ARG mashup you can dream up. At the end of every
loop the arpeggiator re-sniffs that source and locks onto the fresh value
so the riff doesn't crust over.

```cpp
#include "Arpeggiator.h"
#include "EnvelopeFollower.h"

EnvelopeFollower env;        // converts raw audio into a note-ish value
Arpeggiator   arp;
arp.setBaseNoteSource(&env); // arp will sample this at each loop end

void loop() {
  env.update(audioInput);    // keep the follower breathing
  arp.update(midi, cfg, pots); // arp grabs the latest base note here
}
```

That re-sampling is the anti-stale sauce—no more looping last week's
riffs like it's mall music.
