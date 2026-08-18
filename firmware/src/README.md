# Source Files and Test Mayhem

Welcome to the belly of the beast. This folder splits into two gangs:

- **Core firmware** — the `.cpp` files that actually make the MN42 sing. Stuff like `Arpeggiator.cpp`, `ButtonManager.cpp`, and `firmware_main.cpp` get baked into the shipping build.
- **Machine tests** — `*_t.cpp` hardware harnesses you flash and exercise manually. Unity smoke tests live separately in `../test/test_*.cpp`.

## Source ↔ Docs Fast Lane

If you're trying to understand the firmware as a machine instead of chasing one
module in isolation, start with
[firmware_main.cpp](firmware_main.cpp) and the ordered walkthrough in
[../../docs/firmware/FirmwareMainReadingPath.md](../../docs/firmware/FirmwareMainReadingPath.md).

If you're trying to grok what a file is supposed to do, jump straight to its manifesto:

- [Arpeggiator.cpp](Arpeggiator.cpp) → [../include/Arpeggiator/README.md](../include/Arpeggiator/README.md)
- [ButtonManager.cpp](ButtonManager.cpp) → [../include/ButtonManager/README.md](../include/ButtonManager/README.md)
- [ConfigManager.cpp](ConfigManager.cpp) → [../include/ConfigManager/README.md](../include/ConfigManager/README.md)
- [DisplayManager.cpp](DisplayManager.cpp) → [../include/DisplayManager/README.md](../include/DisplayManager/README.md)
- [EnvelopeFollower.cpp](EnvelopeFollower.cpp) → [../include/EnvelopeFollower/README.md](../include/EnvelopeFollower/README.md)
- [LEDManager.cpp](LEDManager.cpp) → [../include/LEDManager/README.md](../include/LEDManager/README.md)
- [MIDIHandler.cpp](MIDIHandler.cpp) → [../include/MIDIHandler/README.md](../include/MIDIHandler/README.md)
- [PotentiometerManager.cpp](PotentiometerManager.cpp) → [../include/PotentiometerManager/README.md](../include/PotentiometerManager/README.md)
- [protocol/Protocol.cpp](protocol/Protocol.cpp) → [protocol/README.md](protocol/README.md)

## Run the Tests

Each test rides its own PlatformIO environment. To see one roar, run the matching command from the `firmware/` directory:

```bash
pio run -e teensy40_full_system               # builds src/main_t.cpp
pio run -e teensy40_unified_test              # builds src/unified_t.cpp
pio run -e teensy40_display_alive             # builds src/display_alive_t.cpp
pio run -e teensy40_biquad_test               # builds src/biquadfilter_t.cpp
pio run -e teensy40_eeprom_persistence        # builds src/eeprom_persistence_t.cpp (LittleFS default)
pio run -e teensy40_slot_verify               # builds src/verify_slots_t.cpp (LittleFS default)
```

Use these to keep your mods honest. Compile, flash, repeat—no fear, no mercy.

## Jitter Tuning

Hold `Ctrl0 + Ctrl3 + Ctrl4` and turn control pots to tune Perlin jitter:
- Pot 0: depth (`Jitter: 0.35`)
- Pot 1: smoothness (`Smooth: 0.42`)

## Live Double-press Shortcuts

Outside config and LFO-tune modes, double-press the last three control buttons to edit the active slot live:

- `Ctrl3`: cycle EF oversampling through 1x/2x/4x/8x/16x/32x.
- `Ctrl4`: toggle ARG on/off while retaining its selected method and sources.
- `Ctrl5`: toggle fixed LFO 1 modulation. A new lane starts Centered at 100%; disabling it retains that tuning.

These are exclusive double gestures with a 300 ms window. The corresponding channel, registry-number, and tap-tempo single actions run only after that window closes. See the [ButtonManager field guide](../include/ButtonManager/README.md) for the complete map.

## MIDI Nerd Notes

`MIDIHandler.cpp` now speaks RPN and can sniff out universal SysEx packets. If you're poking at the MIDI spec, this is your playground to see how the fancy stuff maps to code.
