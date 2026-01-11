# Source Files and Test Mayhem

Welcome to the belly of the beast. This folder splits into two gangs:

- **Core firmware** — the `.cpp` files that actually make the MN42 sing. Stuff like `Arpeggiator.cpp`, `ButtonManager.cpp`, and `firmware_main.cpp` get baked into the shipping build.
- **Machine tests** — files named `test_*.cpp`. They're not here to cuddle; they exist to punch the firmware in the face and make sure it still behaves. These builds mirror the final hardware and yank in the same headers from `../include/`, so you're throwing real punches.

## Source ↔ Docs Fast Lane

If you're trying to grok what a file is supposed to do, jump straight to its manifesto:

- [Arpeggiator.cpp](Arpeggiator.cpp) → [../include/Arpeggiator/README.md](../include/Arpeggiator/README.md)
- [ButtonManager.cpp](ButtonManager.cpp) → [../include/ButtonManager/README.md](../include/ButtonManager/README.md)
- [ConfigManager.cpp](ConfigManager.cpp) → [../include/ConfigManager/README.md](../include/ConfigManager/README.md)
- [DisplayManager.cpp](DisplayManager.cpp) → [../include/DisplayManager/README.md](../include/DisplayManager/README.md)
- [EnvelopeFollower.cpp](EnvelopeFollower.cpp) → [../include/EnvelopeFollower/README.md](../include/EnvelopeFollower/README.md)
- [LEDManager.cpp](LEDManager.cpp) → [../include/LEDManager/README.md](../include/LEDManager/README.md)
- [MIDIHandler.cpp](MIDIHandler.cpp) → [../include/MIDIHandler/README.md](../include/MIDIHandler/README.md)
- [PotentiometerManager.cpp](PotentiometerManager.cpp) → [../include/PotentiometerManager/README.md](../include/PotentiometerManager/README.md)

## Run the Tests

Each test rides its own PlatformIO environment. To see one roar, run the matching command from the `firmware/` directory:

```bash
pio run -e teensy40_mainTEST           # builds test_main.cpp
pio run -e teensy40_unified_test       # builds test_Unified.cpp
pio run -e teensy40_biquad_test        # builds test_biquadfilter.cpp
pio run -e teensy40_eeprom_persistence # builds test_eeprom_persistence.cpp
pio run -e teensy40_slot_verify        # builds test_verify_slots.cpp
```

Use these to keep your mods honest. Compile, flash, repeat—no fear, no mercy.

## Jitter Tuning

Hold `Ctrl0 + Ctrl3 + Ctrl4` and turn control pots to tune Perlin jitter:
- Pot 0: depth (`Jitter: 0.35`)
- Pot 1: smoothness (`Smooth: 0.42`)

## MIDI Nerd Notes

`MIDIHandler.cpp` now speaks RPN and can sniff out universal SysEx packets. If you're poking at the MIDI spec, this is your playground to see how the fancy stuff maps to code.
