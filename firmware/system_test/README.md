# System Test Pit

Welcome to the gauntlet. This folder is where the firmware proves it can survive a night on stage.
These are **hardware-in-the-loop** tests — they don't fake the MCU or the peripherals.
We flash the real board and make sure the buttons, LEDs, EEPROM and friends actually do their job.

## What's inside

Each `test_*.cpp` file boots a slim sketch that hammers a subsystem:

- `test_mainSystem.cpp` – full-stack slapdown covering long presses, EEPROM sanity and MIDI plumbing.
- `test_button_manager.cpp` – makes the button grid earn its keep.
- `test_display_manager.cpp` – pushes pixels and checks the OLED doesn't ghost out.
- `test_led_manager.cpp` – runs the LED driver through color and brightness hoops.
- `test_potentiometer_manager.cpp` – confirms the knobs report honest values.

Shared helpers live in `TestHelpers.cpp`; tweak them if your rig needs extra scaffolding.

## Running a test

Pick an environment from `platformio.ini` that matches the file you're poking. Examples:

```bash
# full integration sweep
pio run -d firmware -e teensy40_full_system

# EEPROM persistence grinder
pio run -d firmware -e teensy40_eeprom_persistence
```

Each build uploads the sketch to the Teensy 4.0. Crack open a serial monitor on **Serial1** and watch the Unity output belt out test results.

## Why bother?

Unit tests catch logic bugs, but these brutes uncover the "oops, forgot the pull-up" disasters.
Run them anytime the hardware changes or before you call the prototype "done".

Rock it, break it, then make it better.
