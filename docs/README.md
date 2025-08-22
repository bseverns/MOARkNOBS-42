# Docs Index

> Hardware's signed off and etched in copper. This folder is the reference stash for the whole machine—schematics, firmware lore, and every misadventure we've documented.

Welcome to the MOARkNOBS-42 documentation playground. This README aims to help you navigate the library of notes, design scraps, and personal ramblings we keep around to teach ourselves and the next hacker.

## Visual Quickies

> Sketches that slap the architecture on a napkin so you don't have to squint at code.

### Everything Everywhere

```mermaid
flowchart TD
  Flash --> Loader[Bootloader] --> FW[Teensy Firmware]
  FW --> BM[ButtonManager]
  Buttons((Buttons)) -->|scan| BM --> MIDIOut[MIDIHandler]
  EF[EnvelopeFollower] -->|mod| MIDIOut
  ARP[Arpeggiator] --> MIDIOut
  MIDIIN --> DM[DisplayManager]
  MIDIIN --> Slots((Slots))
  Browser[[Browser]] --> WebSerial --> FW
  NodeOSC[[Node OSC Bridge]] --> FW
  FW --> MIDIOut((MIDI Out))
```

## System Capabilities

- [Hardware README](../hardware/README.md) — final board layout, power rails, and the gritty bits you can actually solder.
- [Firmware README](../firmware/README.md) — how the code slings MIDI, wrangles envelope followers, and keeps the LEDs honest.
- Firmware reference tables: [button map & combo guide](../firmware/include/ButtonManager/README.md#button-map), [filter types](../firmware/include/EnvelopeFollower/README.md#filter-types), [arp settings](../firmware/include/Arpeggiator/README.md#arp-settings), [MIDI types](../firmware/include/MIDIHandler/README.md#supported-message-types), [ARG methods](../firmware/include/EnvelopeFollower/README.md#arg-methods), [display hooks](../firmware/include/DisplayManager/README.md#key-methods).

## Choose Your Adventure

- [BuildersHandbook.md](BuildersHandbook.md) — wire it, flash it, and smoke-test it.
- [HISTORY.md](HISTORY.md) — chronological ride through the project's evolution. Commit references, design pivots, and the "why" behind the build.
- [Options_DNI.md](Options_DNI.md) — the optional / Do Not Install cheat sheet. Use this before you lock a BOM or when you're deciding what not to solder.
- [TODO.md](TODO.md) — post-release wishlist for when the first build is out and you're itching for v2.
- [sketch/](sketch/) — raw schematics and subsystem scribbles when you need the gory details.
Highlights:
  - [buttonMatrix.md](sketch/systemFlow/hw/buttonMatrix.md) — how the 42-button grid scans its soul.
  - [display.md](sketch/systemFlow/hw/display.md) — wrangling pixels and I²C.
  - [envelopeFE.md](sketch/systemFlow/hw/envelopeFE.md) — analog envelope follower circuits.
  - Plenty more (midi opto, power antics, board PDFs) for late-night study.
- [WebSerial.md](WebSerial.md) — how the board chats with browsers.
- [OSCBridge.md](OSCBridge.md) — hurl OSC at a Node shim and let it punch MIDI into the hardware.
- [thermal/](thermal/) — keep the silicon from frying itself.
