# MOARkNOBS-42 Hardware

> ~~The button board that turns the firmware's dreams into something you can actually solder.~~ Pure DIY attitude. This board design adds ten addressable LEDs—one shadowing each envelope follower input, one glaring at the control buttons, and three haloing the physical pots—and drops in 1/8" Type‑A MIDI jacks alongside the old-school DIN ports.

Start here: [CurrentBuild.md](./CurrentBuild.md) is the canonical source for what hardware files are current, legacy, or still missing from this checkout.

```mermaid
flowchart TD
  Buttons[Button Matrix] --> MUX[CD74HC4067]
  MUX --> Teensy[Teensy 4.0]
  Teensy --> LEDs[WS2812 LEDs]
  Teensy --> MIDI[DIN/TRS MIDI]
  Teensy --> EF[Envelope Inputs]
```

![3D render of MOARkNOBS board with jacks and knobs](../docs/sketch/MOAR_BOARD.png)

Want the whole glam squad—including machine drawings and subsystem sketches? Start with [CurrentBuild.md](./CurrentBuild.md), then dive into [docs/sketch/](../docs/sketch/).[^logic][^midi][^opamp]

## Accessibility

- Silkscreen labels are big and high-contrast so you can read them under garbage lighting.
- LEDs shadow inputs and buttons for visual feedback when audio or touch isn't enough.
- The docs keep alt text and tidy heading levels, so screen readers don't puke trying to parse them.

Need a deeper schematic fix? The [systemflow docs](../docs/sketch/systemFlow/hw/) slice the board into subsystems with all the gnarly traces.

## Specs

- **Microcontroller**: Teensy 4.0 — 600 MHz of ARM punk powering the show. [Why we picked it](./Parts.md#teensy-40).
- **Addressable LEDs**: 52 × WS2812-style — six stalk the envelope followers, one heckles the control buttons, and three crown the pots. [Why we picked them](./Parts.md#ws2812-leds).
- **Envelope Follower Inputs**: 6 analog channels ready for audio or CV (+5 V). [Why the op-amp front-end looks this way](./Parts.md#mcp6002-op-amp).
- **Key Matrix MUX**: Two CD74HC4067s funnel forty‑two switches into a single ADC read. [Why these muxes](./Parts.md#cd74hc4067-analog-mux).
- **Power**: 5 V logic rail plus a 5 V LED rail; core fuse at 0.5 A, LED fuse at 2.5 A. [Why the split rails](./Parts.md#power-rails--fuses).
- **MIDI I/O**: 5‑pin DIN plus 1/8" TRS Type‑A jacks wired in parallel. [Why the opto + TRS combo](./Parts.md#6n138-optocoupler).

The firmware slurps up every one of these hooks—animating LEDs, sampling EFs, and watching the rails. Check the [firmware README](../firmware/README.md) for how the code bends the hardware to its will, grab the [Parts & Rationale](./Parts.md) doc when you need the deeper sourcing story, and use [Substitutions.md](./Substitutions.md) before swapping critical parts.

## Directory Layout

- [CurrentBuild.md](./CurrentBuild.md) – canonical hardware status page.
- [Parts.md](./Parts.md) – part rationale and design intent.
- [Substitutions.md](./Substitutions.md) – conservative sourcing/substitution notes.
- [MN42-machineDrawings/](./MN42-machineDrawings/) – verified machine-drawing PDFs currently present in the repo.
- [fabrication/](./fabrication/) – fabrication folder; no versioned Gerber zip was present in this repo audit.

## Power Rails Need Fat Copper

If you're routing or poking at the LED power network, keep the lifeblood thick:

- `VCC`
- `5V`
- `VLED`
- any other LED power rail hiding in your design

In EasyEDA (or whatever CAD you're rocking), filter on each of those nets, hover the trace, and smash **Change Width** until it's **0.5 mm or wider**. After you push the update, regenerate the Gerbers and verify the top copper export still reflects that minimum width before you send anything to fab. Thin copper means brown-outs and sad pixels, so keep it beefy.

## Fabrication Package

Use [CurrentBuild.md](./CurrentBuild.md) as the release-facing source of truth.

Current repo state from the 2026-03 audit:

- Verified present: `hardware/MN42-machineDrawings/PCB_MOAR_Board_2025-09-03.pdf`
- Verified present: `hardware/MN42-machineDrawings/SCH_MOAR_Schematic_2025-08-30.pdf`
- `TODO: add a versioned fabrication zip under hardware/fabrication/`
- `TODO: add a versioned BOM export under hardware/`
- `TODO: if enclosure CAD is meant to ship in-repo, add a versioned directory and link it from CurrentBuild.md`

Circuit diagrams live in [`sketch/`](../docs/sketch/). System modules have their circuits diagramed to show the board's guts in living color.

![Schematic for interface and control section](../docs/sketch/Interface%26Cntrl.png)
![Schematic for power regulation and protection](../docs/sketch/Power-Reg.png)

### Sketch Documents

* Matrix Diode Array [buttonMatrix.md](../docs/sketch/systemFlow/hw/buttonMatrix.md)
* Power and Protection [power&protection.md](../docs/sketch/systemFlow/hw/power&protection.md)
* PLC/Voltage shifters [teensy&headers.md](../docs/sketch/systemFlow/hw/teensy&headers.md)
* MIDI OUT [led&midiOut.md](../docs/sketch/systemFlow/hw/led&midiOut.md)
* MIDI IN [midiOpto.md](../docs/sketch/systemFlow/hw/midiOpto.md)
* Envelope Follower [envelopeFE.md](../docs/sketch/systemFlow/hw/envelopeFE.md)
* Display [display.md](../docs/sketch/systemFlow/hw/display.md)

### SparkFun Shortcut

If these things seem WAY over your head, don't worry. They were for me too, at first. A cool thing is that SparkFun fabs the Teensy line and sells breakouts for nearly every silicon misfit on this board. Grab a dev board, follow one of their many tutorials, and prototype before you ever open a PCB editor.

| Part | SparkFun Link | What the article covers |
| --- | --- | --- |
| Teensy 4.0 | [Hookup Guide](https://learn.sparkfun.com/tutorials/teensy-40-hookup-guide) | Pinout, power, programming basics |
| WS2812 / NeoPixel LED | [WS2812 Breakout Guide](https://learn.sparkfun.com/tutorials/ws2812-breakout-hookup-guide) | Driving addressable LEDs and power tips |
| CD74HC4067 MUX | [16‑Channel Mux Guide](https://learn.sparkfun.com/tutorials/16-channel-analogdigital-multiplexer-breakout-cd74hc4067-hookup-guide) | Reading 16 channels through one port |
| SN74HCT245 Level Shifter | [Logic Level Shifting 101](https://learn.sparkfun.com/tutorials/logic-level-shifting) | Translating 5 V button buses to 3 V3 logic |
| 6N138 MIDI Opto | [MIDI Tutorial](https://learn.sparkfun.com/tutorials/midi-tutorial/all) | Opto‑isolated MIDI wiring and DIN vs. TRS |
| SSD1306 OLED | [Micro OLED Breakout Guide](https://learn.sparkfun.com/tutorials/micro-oled-breakout-hookup-guide) | I²C wiring and drawing pixels |

#### If this, then that

- Want variable CV routing? Swap in SparkFun's [CD74HC4067 breakout](https://www.sparkfun.com/products/9056) and reroute envelopes without soldering.
- Testing MIDI before committing to the PCB? Their [MIDI Shield](https://www.sparkfun.com/products/12898) drops a ready-made DIN/optocoupler stage on your bench.

[^logic]: SparkFun's [Logic Level Shifting 101](https://learn.sparkfun.com/tutorials/logic-level-shifting) article explains the SN74HCT245 block on sheet 5.
[^midi]: SparkFun's [MIDI Tutorial](https://learn.sparkfun.com/tutorials/midi-tutorial/all) covers opto-isolation and TRS vs. DIN wiring used on sheet 6.
[^opamp]: SparkFun's [Op-Amp Basics](https://learn.sparkfun.com/tutorials/op-amps/all) digs into the rectifier + RC filter behind the envelope follower on sheet 7.

Below is a summary of the schematic sheets:

| Sheet # | Title                                  | Contents                                                                                       |
| ------- | -------------------------------------- | ---------------------------------------------------------------------------------------------- |
| 1       | Title / Block Diagram                  | Topology overview; signal & power flow.                                                        |
| 2       | **Power & Protection**                 | DC jack, F1 logic PTC, TVS, bulk caps, F2 LED PTC, VLED cap, regulators note (Teensy onboard). |
| 3       | **Teensy Core & Headers**              | Teensy 4.0 pinout subset used; I2C to OLED; SPI/unused pads; boot/reset.                       |
| 4       | **Key Matrix & MUX**                   | 42 switches + diodes; 2×CD74HC4067 (or # actually used); row/col nets labelled; OE pull-ups.   |
| 5       | **Level-Shifter & LED / MIDI OUT**     | SN74HCT245, RLED 33 Ω, MIDI OUT loop resistors, DIN + TRS connectors.                                 |
| 6       | **MIDI IN Opto + ESD**                 | 6N138 (or alt), input resistors, 3V3 pull-up, optional activity LED, DIN + TRS jacks.                           |
| 7       | **Envelope Follower Analog Front-End** | 6 channels: audio jack (or header), rectifier, RC, clamp to 3V3, into ADC pins.                |
| 8       | **Display, UI, Aux Headers**           | SSD1306 (I²C), spare expansion header (5V, 3V3, SDA, SCL, GND), debug SWD pads.                |
| 9       | Netlist summary / BOM cross-ref.       |                                                                                                |

### Analog Ground Stitching

The envelope follower front-end is a noise magnet, so we drenched it in a GND pour and pinned that copper down with vias every ~5 mm. Each stitch dives into the main ground plane so the envelope follower keeps quiet. If you mod this section or stretch the board, clone those vias and keep the spacing tight—same net, same vibe. When a versioned fabrication archive is checked into the repo, mirror its drill pattern during review instead of guessing.

## Safety & Liability

This board might only hum on 5 V, but those rails can deliver enough current to toast silicon or your fingertips. Plugging things in backwards, bypassing fuses, or letting stray tools bridge traces can nuke chips, start fires, or light up the room in the worst way. Treat every conductor like it wants to bite—double-check polarity, respect current limits, and never poke live circuits unless you know exactly what you're doing.

Build, mod, or rage against this design at your own risk. We’re not your safety net. As the [CERN OHL v2 warranty disclaimer](LICENSE) bluntly states, this documentation and any hardware you spawn from it come **"as is"** with zero warranties or guarantees. If you cook a bench supply, weld a ring to a ground plane, or shock yourself into next week, that’s on you, not us.

## License

These board blueprints are under the CERN Open Hardware Licence v2 - Strongly Reciprocal. Remix, fab, and share alike. Scope the [LICENSE](LICENSE) in this folder for the full legal spiel, and see [docs/LicenseAndSupport.md](../docs/LicenseAndSupport.md) for the plain-English boundary.
