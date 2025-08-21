# Docs Index

> Hardware's signed off and etched in copper. This folder is the reference stash for the whole machine—schematics, firmware lore, and every misadventure we've documented.

Welcome to the MOARkNOBS-42 documentation playground. This README aims to help you navigate the library of notes, design scraps, and personal ramblings we keep around to teach ourselves and the next hacker.

## Visual Quickie

> One diagram, three stories: boot mayhem, runtime crosstalk, and how the bridge jams.

```mermaid
graph TD
  subgraph Boot_Ruckus
    Flash --> Loader[Bootloader]
    Loader --> FW[Firmware]
    FW --> Mods[Modules]
  end
  subgraph Runtime_Noise
    Buttons -->|scan| BM[ButtonManager]
    BM --> MIDI[MIDIHandler]
    EF[EnvelopeFollower] -->|mod| MIDI
    ARP[Arpeggiator] --> MIDI
    MIDI --> DM[DisplayManager]
    MIDI --> Slots
  end
  subgraph Bridge_Paths
    Browser --> WebSerial --> Board[Teensy]
    NodeOSC[[Node OSC Bridge]] --> Board
    Board --> MIDIOut
  end
  FW --> BM
  Board --> FW
```

## System Capabilities

- [Hardware README](../hardware/README.md) — final board layout, power rails, and the gritty bits you can actually solder.
- [Firmware README](../firmware/README.md) — how the code slings MIDI, wrangles envelope followers, and keeps the LEDs honest.
- Firmware reference tables: [button map](../firmware/include/ButtonManager/README.md#button-map), [filter types](../firmware/include/EnvelopeFollower/README.md#filter-types), [arp settings](../firmware/include/Arpeggiator/README.md#arp-settings), [MIDI types](../firmware/include/MIDIHandler/README.md#supported-message-types), [ARG methods](../firmware/include/EnvelopeFollower/README.md#arg-methods), [display hooks](../firmware/include/DisplayManager/README.md#key-methods).

## Choose Your Adventure

- [BuildersHandbook.md](BuildersHandbook.md) — wire it, flash it, and smoke-test it.
- [HISTORY.md](HISTORY.md) — chronological ride through the project's evolution. Commit references, design pivots, and the "why" behind the build.
- [Options_DNI.md](Options_DNI.md) — the optional / Do Not Install cheat sheet. Use this before you lock a BOM or when you're deciding what not to solder.
- [TODO.md](TODO.md) — post-release wishlist for when the first build is out and you're itching for v2.
- [DemoBlockers.md](DemoBlockers.md) — the pre-demo punch list; burn this down before you show it to strangers.
- [sketch/](sketch/) — raw schematics and subsystem scribbles when you need the gory details.
Highlights:
  - [buttonMatrix.md](sketch/buttonMatrix.md) — how the 42-button grid scans its soul.
  - [display.md](sketch/display.md) — wrangling pixels and I²C.
  - [envelopeFE.md](sketch/envelopeFE.md) — analog envelope follower circuits.
  - Plenty more (midi opto, power antics, board PDFs) for late-night study.
- [WebSerial.md](WebSerial.md) — how the board chats with browsers.
- [OSCBridge.md](OSCBridge.md) — hurl OSC at a Node shim and let it punch MIDI into the hardware.
- [thermal/](thermal/) — keep the silicon from frying itself.

## Cross-Pollination

- HISTORY tracks design moves that show up as options in `Options_DNI.md`.
- The sketch notes reference those options when routing or debugging.

## Your Turn

To drop a new sketch, stash a Markdown file in `sketch/` with a punchy intro. Want to memorialize a breakthrough or fiasco? Append a dated note in `HISTORY.md` with the commit that started the fire. PR it and you're part of the legend.

Read, tweak, repeat. And if the docs don't answer it, that's your cue to write the next page—preferably with a soldering iron in hand.
