# Contributor Guidelines

## Build and Test

- Use [PlatformIO](https://platformio.org/) to compile the firmware. Run builds from the `firmware/` directory.
- Main firmware build:
  ```bash
  pio run -e teensy40_main
  ```
- Available full machine test environments (build with `pio run -e <env>`):
  - `teensy40_full_system`
  - `teensy40_unified_test`
  - `teensy40_biquad_test`
  - `teensy40_eeprom_persistence`
  - `teensy40_slot_verify`

- Software Test:
  - `teensy40_unity` (for Unity tests)

## Coding Standards

- Declare global variables as `extern` in header files and define them exactly once in a corresponding `.cpp` file.
- Keep code comments up to date. If you change any public API, document the change in `README.md`.

## REPO CONTRACT — MOARkNOBS-42 (strict)

• PlatformIO project root is ./firmware. Never treat repo root as a PIO project.
• Tests run with: pio -d firmware test -e teensy40_unity --without-uploading --without-testing -vvv
• Unity transport is CUSTOM ONLY. Do not rely on or regenerate PlatformIO’s Serial-based transport.
  - Required symbols provided by firmware/test/unity_output.cpp:
    unityOutputStart/Char/Flush/Complete → use Serial1 (NOT Serial).
  - Never add -DSerial=… or include unittest_transport.h.
• Teensy USB mode for tests is USB_MIDI_SERIAL. Do not introduce or re-enable conflicting USB_* defines.
• During edits, output a single unified diff touching only the files I list. Do not invent files or rename paths.
• Don’t pull display (Adafruit GFX/SSD1306/BusIO) or SD/SdFat into the unit-test env unless explicitly requested.
• If uncertain, ask one clarifying question before changing code.