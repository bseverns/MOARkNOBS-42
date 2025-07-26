# Contributor Guidelines

## Build and Test

- Use [PlatformIO](https://platformio.org/) to compile the firmware. Run builds from the `firmware/` directory.
- Main firmware build:
  ```bash
  pio run -e teensy40_main
  ```
- Available test environments (build with `pio run -e <env>`):
  - `teensy40_mainTEST`
  - `teensy40_unified_test`
  - `teensy40_biquad_test`
  - `teensy40_eeprom_persistence`
  - `teensy40_slot_verify`

## Coding Standards

- Declare global variables as `extern` in header files and define them exactly once in a corresponding `.cpp` file.
- Keep code comments up to date. If you change any public API, document the change in `README.md`.

