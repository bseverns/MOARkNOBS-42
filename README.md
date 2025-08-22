# MOARkNOBS-42

![Title Image](docs/land.png)
> The button-mashing, knob-twisting controller that refuses to behave.

This repo mashes firmware, hardware, docs, and a scrappy bridge into one place.
The top README is intentionally barebones—poke the READMEs in each folder for
the full scoop.

## Quick Start

Ready to shred? Here's the bare minimum to get the beast humming.

### Dependencies

- Python bits: `pip install -r requirements.txt`
- Node 18+ for the bridge (LTS or go home)
- PlatformIO on your PATH

### Firmware build

```bash
pio run -e teensy40_main
```

### Test run

```bash
pio -d firmware test -e teensy40_unity -vvv
npm --prefix bridge test
```

Want the deep cuts? The full chronicles live in [docs/](docs/README.md) and
the gritty firmware lore is in [firmware/](firmware/README.md).

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
- Button grid that refuses to behave—short, long, double and combo presses are all mapped in the [ButtonManager cheat sheet](firmware/include/ButtonManager/README.md#button-map).
- MIDI chops, ARG math, and OLED tricks are mapped out in their own module tables.
- Dual MIDI jacks—5‑pin DIN for the old heads and 1/8" TRS Type‑A for anyone who left their big cables at home.

## Diagram Dump

> Because ascii tables only go so far. Here's how the beast actually routes its noise.

### One Big Flow

```mermaid
flowchart LR
  subgraph Userland
    User((User))
    Grid[[Button Grid]]
  end
  subgraph Host
    HostPC((Host))
    WebSerial[[WebSerial Editor]]
    Bridge[[OSC Bridge]]
  end
  subgraph Board
    Firmware[Teensy 4.0 Firmware]
    Hardware[[Knobs & LEDs]]
    MIDIOut((MIDI Out))
    LFO1((LFO1))
    Env((Envelope Follower))
    Slot42((Slot 42))
  end
  User -->|mash| Grid -->|scan| Firmware
  HostPC --> WebSerial --> Firmware
  HostPC --> Bridge --> Firmware
  Firmware <--> Hardware
  Env -->|tugs| LFO1 -->|shakes| Slot42 -->|spits| MIDIOut
  Firmware --> Slot42
```

Need the dirt? Dive into the sub-READMEs and get lost.

License: MIT. See [LICENSE](LICENSE).

## Contributing

Want in on the mayhem? `pre-commit` now guards the gates with `clang-format`,
ESLint and Prettier. Run `pre-commit install` once and it'll slap your changes
into shape every commit.

