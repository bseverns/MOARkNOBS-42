# Hardware

Hardware artifacts live in `hardware/` and supporting diagrams in `docs/sketch/`.

## Core board characteristics

- Teensy 4.0 controller
- 42-button matrix via multiplexers
- 3 physical pots with virtual slot architecture
- 52 WS2812 LEDs
- 6 envelope follower inputs
- MIDI I/O over DIN and TRS Type-A

## Build and fabrication assets

- Machine drawings: `hardware/MN42-machineDrawings/`
- PCB package PDF: `hardware/MN42-machineDrawings/PCB_MOAR_Board_2025-09-03.pdf`
- Schematic PDF: `hardware/MN42-machineDrawings/SCH_MOAR_Schematic_2025-08-30.pdf`
- Fabrication workspace: `hardware/fabrication/` (populate with current Gerber/BOM exports per release)

## Design docs

- Hardware overview: `hardware/README.md`
- Part rationale: `hardware/Parts.md`
- System flow sketches: `docs/sketch/systemFlow/hw/`
- Pin mapping: `docs/PinMap.md`

## Electrical and safety notes

- Keep LED power rails wide (>= 0.5mm traces in PCB CAD revisions).
- Respect fused rails and common ground wiring practices.
- Verify power polarity before first boot and rework.
