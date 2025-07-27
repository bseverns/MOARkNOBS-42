# Header Overview

> Interface declarations and helper bits used across the MN42 firmware.
> If you're hacking on the code, start here.

This folder holds the C++ headers that glue the controller together.
Each file corresponds to a manager class or shared structure.
Below is a quick cheat sheet.

## Files

- **Arpeggiator.h** – simple timed note repeater for any slot.
- **BiquadFilter.h** – lightweight filter used by the envelope followers.
- **ButtonManager.h** – scans the button matrix and debounces presses.
- **ConfigManager.h** – reads/writes configuration to EEPROM.
- **DisplayManager.h** – wrappers around the SSD1306 OLED display.
- **EnvelopeFollower.h** – tracks audio or CV to modulate slots.
- **Globals.h** – compile-time constants and forward declarations.
- **LEDManager.h** – drives the addressable LED strip.
- **MIDIHandler.h** – thin wrapper for USB and DIN MIDI I/O.
- **MIDITypes.h** – enums and structs defining slot data.
- **PotentiometerManager.h** – reads analog pots via multiplexers.
- **TestHelpers.h** – small helpers used by the manual test firmware.
- **Utility.h** – common math helpers and a lightweight task scheduler.
- **name.c** – sets the custom USB MIDI product string.

For the full firmware story see [../README.md](../README.md).
