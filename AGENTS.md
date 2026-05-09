# Contributor Guidelines

## Build and Test

- Use [PlatformIO](https://platformio.org/) to compile the firmware. The PlatformIO project root is `firmware/`; from repo root, always pass `-d firmware`.
- Main firmware build:
  ```bash
  pio run -d firmware -e teensy40_main
  ```
- Available full machine test environments (build with `pio run -e <env>`):
  - `teensy40_full_system`
  - `teensy40_unified_test`
  - `teensy40_biquad_test`
  - `teensy40_eeprom_persistence`
  - `teensy40_slot_verify`

- Software Test:
  ```bash
  pio test -d firmware -e teensy40_unity -vvv
  ```

- Guardrail checks from the repo root:
  ```bash
  python3 tools/check_contract_sync.py --root .
  python3 tools/check_control_coverage.py --root .
  python3 tools/check_release_readiness.py --root . --stage hardware-test
  npm --prefix App test
  npm --prefix bridge test
  ```

- Grouped local checks:
  ```bash
  python3 tools/doctor.py --docs
  python3 tools/doctor.py --app
  python3 tools/doctor.py --bridge
  python3 tools/doctor.py --release
  python3 tools/doctor.py --firmware
  python3 tools/doctor.py --full
  ```

## Coding Standards

- Declare global variables as `extern` in header files and define them exactly once in a corresponding `.cpp` file.
- Keep code comments up to date. If you change any public API, document the change in `README.md`.
- Markdown docs must be accessible: alt text on images, sequential headings, and no color-only cues.

## REPO CONTRACT — MOARkNOBS-42 (strict)

• PlatformIO project root is ./firmware. Never treat repo root as a PIO project.
• Tests run with: pio test -d firmware -e teensy40_unity -vvv
• Unity transport is CUSTOM ONLY. Do not rely on or regenerate PlatformIO’s Serial-based transport.
  - Required symbols provided by firmware/test/unity_output.cpp:
    unityOutputStart/Char/Flush/Complete → use Serial1 (NOT Serial).
  - Never add -DSerial=… or include unittest_transport.h.
• Teensy USB mode for tests is USB_MIDI_SERIAL. Do not introduce or re-enable conflicting USB_* defines.
• Don’t pull display (Adafruit GFX/SSD1306/BusIO) or SD/SdFat into the unit-test env unless explicitly requested.
• If uncertain, ask one clarifying question before changing code.
