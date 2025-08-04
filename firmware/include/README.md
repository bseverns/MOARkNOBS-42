# Header Overview

> Interface declarations and helper bits used across the MN42 firmware.
> If you're hacking on the code, start here.

This folder holds the C++ headers that glue the controller together.
Each file corresponds to a manager class or shared structure.
Below is a quick cheat sheet.

## Files

- **Arpeggiator.h** – simple timed note repeater for any slot ([Arpeggiator.cpp](../src/Arpeggiator.cpp)).
- **BiquadFilter.h** – lightweight filter used by the envelope followers.
- **ButtonManager.h** – scans the button matrix and debounces presses ([ButtonManager.cpp](../src/ButtonManager.cpp)).
- **ConfigManager.h** – reads/writes configuration to EEPROM ([ConfigManager.cpp](../src/ConfigManager.cpp)).
- **DisplayManager.h** – wrappers around the SSD1306 OLED display ([DisplayManager.cpp](../src/DisplayManager.cpp)).
- **EnvelopeFollower.h** – tracks audio or CV to modulate slots ([EnvelopeFollower.cpp](../src/EnvelopeFollower.cpp)).
- **Globals.h** – compile-time constants and forward declarations ([Globals.cpp](../src/Globals.cpp)).
- **hardware_config.h** – empty stage where `applyHardwareConfigOverrides()` can thrash default pins and ticks into your rig's groove.
- **LEDManager.h** – drives the 52-piece addressable LED circus: slot halos, envelope meters, pot glows and the control-button beacon ([LEDManager.cpp](../src/LEDManager.cpp)).
- **MIDIHandler.h** – thin wrapper for USB and DIN MIDI I/O ([MIDIHandler.cpp](../src/MIDIHandler.cpp)).
- **MIDITypes.h** – enums and structs defining slot data.
- **PotentiometerManager.h** – reads analog pots via multiplexers ([PotentiometerManager.cpp](../src/PotentiometerManager.cpp)).
- **TestHelpers.h** – small helpers used by the manual test firmware.
- **Utility.h** – common math helpers and a lightweight task scheduler ([Utility.cpp](../src/Utility.cpp)).
- **WebSerial.h** – ships slot snapshots over USB for the web editor ([WebSerial.cpp](../src/WebSerial.cpp)).
- **name.c** – sets the custom USB MIDI product string.

## How these pieces jam together

- `ButtonManager` hollers when a switch gets smacked and `LEDManager` answers with a light show, while `DisplayManager` scribbles the update on the OLED.
- `PotentiometerManager` pours raw knob juice into `ConfigManager`, which then nudges `LEDManager` and `DisplayManager` so your fingers see what your ears are about to hear.
- `MIDIHandler` routes incoming notes and clock, letting `Arpeggiator` and `EnvelopeFollower` sync their mischief; `BiquadFilter` keeps the follower's wiggles smooth.
- `WebSerial` taps `ConfigManager` to fling slot snapshots over USB for external editors or debugging.

For the full firmware story see [../README.md](../README.md).
