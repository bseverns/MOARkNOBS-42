# MOARkNOBS-42

![Title Image](docs/land.png)
> The button-mashing, knob-twisting controller that refuses to behave.

This repo mashes firmware, hardware, docs, and a scrappy bridge into one place.
The top README is intentionally barebones—poke the READMEs in each folder for
the full scoop.

## Quick Map

| Folder | What's in it |
| --- | --- |
| [docs/](docs/README.md) | Build notes, history and assorted rants |
| [firmware/](firmware/README.md) | Teensy 4.0 codebase. Tables for [buttons](firmware/include/ButtonManager/README.md#button-map), [filters](firmware/include/EnvelopeFollower/README.md#filter-types), [arp settings](firmware/include/Arpeggiator/README.md#arp-settings), [MIDI types](firmware/include/MIDIHandler/README.md#supported-message-types), [ARG methods](firmware/include/EnvelopeFollower/README.md#arg-methods) and [display hooks](firmware/include/DisplayManager/README.md#key-methods) live here |
| [hardware/](hardware/README.md) | Schematics, BOM and enclosure bits |
| [bridge/](bridge/README.md) | Node.js shim that slings serial into OSC/WebMIDI |

## Highlights

- 42 virtual slots and six envelope followers ready to hijack any MIDI stream.
- Built‑in arpeggiator and filter playground.
- WebSerial editor and OSC bridge for remote tweaking.
- Button grid that does far more than it should—see the [ButtonManager table](firmware/include/ButtonManager/README.md#button-map) for the full mischief.
- MIDI chops, ARG math, and OLED tricks are mapped out in their own module tables.
- Dual MIDI jacks—5‑pin DIN for the old heads and 1/8" TRS Type‑A for anyone who left their big cables at home.

## Diagram Dump

> Because ascii tables only go so far. Here's how the beast actually routes its noise.

### System Wiring

```mermaid
graph LR
  Host((Host Computer))
  WebSerial[[WebSerial Editor]]
  Bridge[[OSC Bridge]]
  Firmware[Teensy 4.0 Firmware]
  Hardware[[Knobs & Buttons]]
  Host --> WebSerial --> Firmware
  Host --> Bridge --> Firmware
  Firmware <--> Hardware
```

### Control Burst

```mermaid
sequenceDiagram
  participant User
  participant Grid as ButtonGrid
  participant Code as Firmware
  participant MIDI as MIDIOut
  User->>Grid: mash button
  Grid->>Code: scan & report
  Code->>Code: mod matrix chaos
  Code->>MIDI: blast CC/Note
```

### Mod Matrix Teaser

```mermaid
graph TD
  LFO1((LFO1)) -->|shakes| Slot42
  Env((Envelope Follower)) -->|tugs| LFO1
  Slot42 -->|spits| MIDIOut
```

Need the dirt? Dive into the sub-READMEs and get lost.

License: MIT. See [LICENSE](LICENSE).

