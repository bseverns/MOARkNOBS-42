# MOARkNOBS-42

DIY MIDI controller with **42 virtual control slots**, envelope followers and plenty of button-driven chaos. This repo contains both the firmware and the EasyEDA Pro hardware designs.

## Firmware Highlights

See [`firmware/README.md`](firmware/README.md) for the full manual. Key features include:

- 42 virtual MIDI slots storing channel, CC number and type.
- One main control pot and two additional pots for filter tuning.
- Six real-time envelope followers with seven selectable filter types (linear, opposite, exponential, random, LPF, HPF, BPF).
- Button matrix (42 slot buttons plus 6 control buttons) supporting short, long and double presses.
- ARG mode to blend or compare envelope signals.
- Arpeggiator mode controllable via filter knobs.
- Dual USB & DIN MIDI output.
- OLED display and addressable LEDs for immediate visual feedback.
- Settings stored in EEPROM with automatic backup.
- Configuration via a WebSerial HTML editor.

## Hardware Files

The `hardware/` directory contains design files for the button matrix interface PCB:

- **BTN_42/** – houses the `MN42-1` project with BOM spreadsheets and `Gerber_btnBRD_2025-04-17.zip` for fabrication.

Use this directory to manufacture the hardware or modify the design.

## Getting Started

1. Build and flash the firmware in `firmware/` using PlatformIO or the Arduino IDE.
2. Order or assemble the PCBs from the files in `hardware/`.
3. Wire up the buttons, LEDs and display, then start tweaking.

## License

MOARkNOBS Controller firmware and hardware design files are provided under the MIT License. See the [LICENSE](LICENSE) file for details.

## Author

Designed and developed by the BSSS project team.

