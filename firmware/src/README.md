# Source Files and Test Mayhem

Welcome to the belly of the beast. This folder splits into two gangs:

- **Core firmware** — the `.cpp` files that actually make the MN42 sing. Stuff like `Arpeggiator.cpp`, `ButtonManager.cpp`, and `firmware_main.cpp` get baked into the shipping build.
- **Machine tests** — files named `test_*.cpp`. They're not here to cuddle; they exist to punch the firmware in the face and make sure it still behaves.

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
