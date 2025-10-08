# Arpeggiator

Part of the firmware `include` jungle. The [include README](../README.md) shows how it locks to the rest of the circus; the [main firmware README](../../README.md) explains why the band even exists.

Clock-synced riff machine that rips through a slot's note stack like it's late for soundcheck.
Now it can yank its root note from wherever you tell it—slot memory, a callback, or some mystery source you cooked up—without accidentally sniffing the pot when you told it not to.

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
- `setBaseNoteSource(src)` – choose who owns the root (`Pot`, `Slot`, or `External`).
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
| Base Note Source | Pot, Slot, External | Where the root comes from |
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

## Root note pecking order

The arpeggiator now plays nice with three possible root feeds and only
touches the pot when you explicitly hand it the reins:

1. **Knob life (`BaseNoteSource::Pot`)** – default mode. We read the slot
   pot, map it to MIDI, and pump that straight out. The resulting note is
   mirrored back into `slot.arpNote` so LEDs and displays stay honest.
2. **Slot memory (`BaseNoteSource::Slot`)** – skips the pot entirely and
   trusts whatever the slot last stored. We still write the emitted root
   back so everyone else hears the same gospel.
3. **External source (`BaseNoteSource::External`)** – first hits the
   callback you registered with `setBaseNoteCallback()`. If you skipped
   the callback, it falls back to the last MIDI value you stuffed in via
   `setBaseNote()`. Only if both are missing do we raid the pot as a
   desperation move.

Because the emitted root always syncs back into the slot, other modules
still see the latest note even if it came from some external wizardry.

```cpp
Arpeggiator arp;
arp.setBaseNoteSource(Arpeggiator::BaseNoteSource::External);
arp.setBaseNoteCallback([] { return 64; });
arp.start(0);

void loop() {
  arp.update(midi, cfg, pots); // locks to callback first, pot only if you're flying blind
}
```

That ordering keeps the groove honest: callbacks stay in control, slot
storage stays current, and the pot only chimes in when nothing else is on
the mic.
