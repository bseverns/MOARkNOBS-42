# MOARkNOBS-42

> The button-mashing, knob-twisting controller that refuses to behave.

This repo bundles the firmware, hardware designs and documentation for the **MOARkNOBS-42** project. If you're after the gritty details, dive into the subdirectories below. The latest board rev adds ten more WS2812s, bringing the grand total to fifty‑two LEDs: forty‑two for virtual slots, six tracking envelope follower levels, one beacon for the control buttons and three haloing the hardware knobs—one slot pot and a pair of filter‑tuning lights. As of this rev the box speaks NRPN and spits raw SysEx, so your DAW can't hide behind stock CCs anymore.

Fresh to the scene and itching to see blinkenlights? Crack open the [Builder's Handbook](docs/BuildersHandbook.md) to wire it, flash it, and run first sanity checks before the solder fumes settle.

![Interface & LED Schematic](docs/sketch/PNG_MOAR_Schematic/SCH_MOAR_Schematic_1-INTERFACE-LED-MIDI-CNTRL.png)

## Where's what

- **docs/** – Builder's Handbook, history log, WebSerial guide, and thermal rants.
- **firmware/** – Teensy 4.0 source and project files. The full manual lives in [firmware/README.md](firmware/README.md).
  - **App/** – simple WebSerial editor to tweak settings over USB.
  - **test/** – manual hardware test suite with its own [README](firmware/test/README.md).
- **hardware/** – PCB and enclosure docs. Check [hardware/README.md](hardware/README.md) for the full tour.
  - **MN42-1/** – first board rev; a crash course in how all forty‑two buttons and their misfit LEDs get along with their co-mingled power and data lines. Block diagrams and schematics chill under [`docs/sketch/`](docs/sketch/).
- **[HISTORY.md](docs/HISTORY.md)** – running log of how this project came to be.

## Development Timeline

The timeline reads like a diary of questionable decisions. For the month-by-month breakdown, see [docs/HISTORY.md](docs/HISTORY.md).

## License & Redistribution

MIT, see [LICENSE](LICENSE) for details.

### Redistribution Terms

If you sling this firmware or ship a kit, bundle the `firmware/LICENSES/` directory and either stash the EEPROM source or point to https://github.com/PaulStoffregen/cores/tree/master/teensy4 so the LGPL folks stay cool.

## Author

BSSS project team.

## Thanks

To all of you. You've all made this better whether you realize it or not. Thank you all. Especially Gary.
