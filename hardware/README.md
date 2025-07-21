# Hardware Overview

This directory contains the hardware design files for the MOARkNOBS controller.
All PCBs were created in EasyEDA Pro.

## BTN_42

The `BTN_42` folder hosts revision `MN42-1` of the button board and enclosure.
Key subcircuits are documented as simple flowchart diagrams in
[`BTN_42/sketch/`](BTN_42/sketch/):

- [buttonMatrix.md](BTN_42/sketch/buttonMatrix.md)
- [display.md](BTN_42/sketch/display.md)
- [envelopeFE.md](BTN_42/sketch/envelopeFE.md)
- [led&midiOut.md](BTN_42/sketch/led&midiOut.md)
- [midiOpto.md](BTN_42/sketch/midiOpto.md)
- [power&protection.md](BTN_42/sketch/power&protection.md)
- [teensy&headers.md](BTN_42/sketch/teensy&headers.md)

PNG screenshots of the EasyEDA schematic can be found in
[`BTN_42/sketch/PNG_btnBRD_2025-07-20/`](BTN_42/sketch/PNG_btnBRD_2025-07-20/).

Below is a quick reference for the EasyEDA schematic sections:

| Sheet # | Title                                  | Contents                                                                                       |
| ------- | -------------------------------------- | ---------------------------------------------------------------------------------------------- |
| 1       | Title / Block Diagram                  | Topology overview; signal & power flow.                                                        |
| 2       | **Power & Protection**                 | DC jack, F1 logic PTC, TVS, bulk caps, F2 LED PTC, VLED cap, regulators note (Teensy onboard). |
| 3       | **Teensy Core & Headers**              | Teensy 4.0 pinout subset used; I2C to OLED; SPI/unused pads; boot/reset.                       |
| 4       | **Key Matrix & MUX**                   | 42 switches + diodes; 2×CD74HC4067 (or # actually used); row/col nets labelled; OE pull-ups.   |
| 5       | **Level-Shifter & LED / MIDI OUT**     | SN74HCT245, RLED 33 Ω, MIDI OUT loop resistors, DIN connector.                                 |
| 6       | **MIDI IN Opto + ESD**                 | 6N138 (or alt), input resistors, 3V3 pull-up, optional activity LED.                           |
| 7       | **Envelope Follower Analog Front-End** | 6 channels: audio jack (or header), rectifier, RC, clamp to 3V3, into ADC pins.                |
| 8       | **Display, UI, Aux Headers**           | SSD1306 (I²C), spare expansion header (5V, 3V3, SDA, SCL, GND), debug SWD pads.                |
| 9       | Netlist summary / BOM cross-ref.       |                                                                                                |
