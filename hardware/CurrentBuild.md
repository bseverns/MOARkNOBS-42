# Current Hardware Build

This is the hardware status page for the hardware-test package.

Last audited against repo contents: 2026-06-28.

## Package Scope

This package supports prototype bench validation.
It does not claim fabrication readiness.

## Physical Board Status

Prototype PCBs have been received and are now part of the hardware-test loop. The current work is bring-up evidence,
not production sign-off: power-rail checks, OLED/control validation, MIDI path testing, envelope follower behavior,
LED load testing, and host connectivity through the WebSerial App and bridge.

The fabricated boards are clean enough to make the prototype review useful. Current faults and caveats are being treated
as design and integration findings in this machine unless later bench evidence points to a fabrication-quality problem.

Board photos currently present:

- top-side photo: `docs/assets/board/prodTOP.jpg`
- bottom-side photo: `docs/assets/board/prodBTM.jpg`
- trace-inspection photo: `docs/assets/board/trace.jpg`

Photo TODOs before a full project post:

- assembled front panel with knobs, buttons, OLED, and LEDs installed
- bench setup during first-power and rail-topology validation
- MIDI, USB, and WebSerial/bridge test session with the board connected
- any required rework close-up, or a clear "no rework required" board close-up after validation

## Current Hardware References

| Item | Path | Status | Use in this package |
| --- | --- | --- | --- |
| Schematic reference | `hardware/MN42-machineDrawings/SCH_MOAR_Schematic_2025-08-30.pdf` | present | Current schematic reference for bench validation |
| PCB/reference drawing | `hardware/MN42-machineDrawings/PCB_MOAR_Board_2025-09-03.pdf` | present | Current board drawing reference for bench validation |
| Prototype BOM export | `hardware/fabrication/BOM_MOAR_MOAR_Board_2026-03-17.csv` | present | Current BOM export for prototype parts review |
| Gerber archive | `hardware/fabrication/Gerber_MOAR_Board_1_2026-02-24.zip` | present but unverified | Review-only artifact; not claimed as a verified fabrication bundle |
| Physical board photos | `docs/assets/board/` | present | Public breadcrumb images for current prototype board review |

## How To Use This Folder

1. Use the schematic PDF and PCB/reference drawing PDF to confirm the assembled prototype matches the expected wiring and layout.
2. Use the BOM export, [Parts.md](Parts.md), and [Substitutions.md](Substitutions.md) to review parts and likely substitutions before bench work.
3. If you are preparing manufacturing files, stop. This package does not certify a fabrication-ready output set.

## Known Design / Integration Findings

- The physical boards are suitable for probing and bring-up; no fabrication-quality blocker is claimed from the current
  photo and trace-inspection evidence.
- Rail topology and high-current LED behavior still need dated bench evidence before this prototype can be described as
  release-ready.
- Any fault found during bring-up should be logged as a design/integration finding first, then reclassified only if direct
  inspection or measurement points to fabrication.

## What Is Not Verified Yet

- no verified Gerber plus NC-drill release bundle
- no claim that the current fabrication archive has completed release-level checks
- no order-ready manufacturing sign-off from this package
- no claim that the physical prototype has completed full release-level hardware validation
