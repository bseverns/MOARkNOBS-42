# MOARkNOBS-42

> The button-mashing, knob-twisting controller that refuses to behave.

This repo bundles the firmware, hardware designs and documentation for the **MOARkNOBS-42** project. If you're after the gritty details, dive into the subdirectories below.

![BTN_42 Schematic](docs/sketch/PNG_btnBRD_2025-07-22/SCH_btnBRD_1-btnBRD_2025-07-22.png)

## Where's what

- **firmware/** – Teensy 4.0 source and project files. The full manual lives in [firmware/README.md](firmware/README.md).
  - **App/** – simple WebSerial editor to tweak settings over USB.
  - **test/** – manual hardware test suite with its own [README](firmware/test/README.md).
- **hardware/** – PCB and enclosure docs. Check [hardware/README.md](hardware/README.md) for the overview.
  - **BTN_42/** – button board design; more notes in [BTN_42/README.md](hardware/BTN_42/README.md). Block diagrams and schematics are under [`sketch/`](hardware/BTN_42/sketch).
 - **[HISTORY.md](docs/HISTORY.md)** – running log of how this project came to be.

## Development Timeline

The timeline reads like a diary of questionable decisions. For the month-by-month breakdown, see
[docs/HISTORY.md](docs/HISTORY.md).

## Getting Started

1. **Flash the Firmware**
   - **PlatformIO**: install it via `pip install platformio` or grab the VS Code extension. Change into the `firmware/` directory, pick the `teensy40_main` environment and run `pio run -t upload` with your Teensy 4.0 connected.
   - **Arduino IDE**: install the Teensy board package (`Teensyduino`) and the same libraries listed in `platformio.ini` (FastLED, Bounce2, USB-MIDI, Adafruit SSD1306, Adafruit GFX Library, TimerOne and EEPROM). Open `firmware_main.cpp` as a sketch and upload normally.
2. Order or assemble the PCBs from the files in `hardware/`.
3. Wire up the buttons, LEDs and display, then start tweaking.

### Manual Hardware Tests

Compile the test environments when you want to verify the board outside of the
main firmware:

```bash
pio run -e teensy40_mainTEST      # or teensy40_unified_test, etc.
```

Available environments:

- `teensy40_mainTEST` – step-through checks of each subsystem
- `teensy40_unified_test` – full integration test
- `teensy40_biquad_test` – biquad filter calibration
- `teensy40_eeprom_persistence` – EEPROM backup/restore test
- `teensy40_slot_verify` – verifies MIDI slot storage

See [firmware/test/README.md](firmware/test/README.md) for details on this project's testing suite.

For a month-by-month look at how this controller came together, see
[HISTORY.md](HISTORY.md).

## License

MIT, see [LICENSE](LICENSE) for details.

## Author

BSSS project team.

## Thanks

To all of you. You've all made this better whether you realize it or not. Thank you all.
