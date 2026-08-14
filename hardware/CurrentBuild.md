# Current Hardware Build

This is the hardware status page for the hardware-test package.

Last audited against repo contents: 2026-07-08.

## Package Scope

This package supports prototype bench validation.
It does not claim fabrication readiness.
The tracked `hardware/fabrication/` tree currently contains only `README.md`, which is a boundary note rather than a manufacturing artifact.

## Physical Board Status

Prototype PCBs have been received and are now part of the hardware-test loop. The current work is bring-up evidence,
not production sign-off: power-rail checks, OLED/control validation, MIDI path testing, envelope follower behavior,
LED load testing, and host connectivity through the WebSerial App and bridge.

The fabricated boards are clean enough to make the prototype review useful. Current faults and caveats are being treated
as design and integration findings in this machine unless later bench evidence points to a fabrication-quality problem.
The 0.5 mm test pads visible across the current board are usable for probing during this bench-review phase.

Board photos currently present:

- top-side photo: `docs/assets/board/prodTOP.jpg`
- bottom-side photo: `docs/assets/board/prodBTM.jpg`
- trace-inspection photo: `docs/assets/board/trace.jpg`
- assembled bench-context photo: `docs/assets/board/bench.jpg`
- powered bring-up context photo: `docs/assets/board/bringup.jpg`
- early Teensy breadboard harness photo: `docs/assets/board/test.jpg`
- raw historical assembly-photo archive: `hardware/archive/T-3D5W706139A/`

These images are public breadcrumbs for the prototype story. They do not replace measured inspection notes, serial logs,
or dated bench receipts.

## Current Hardware References

| Item | Path | Status | Use in this package |
| --- | --- | --- | --- |
| Power/button/LED schematic | [sheet 1](../docs/assets/board/SCH_MOAR_Schematic_1-PWR-BUTTON-LED_2026-08-13.png) | present | Power, button-matrix, and LED reference for bench validation |
| Interface/MIDI/control schematic | [sheet 2](../docs/assets/board/SCH_MOAR_Schematic_2-INTERFACE-MIDI-CNTRL_2026-08-13.png) | present | Teensy interface, MIDI, OLED, and direct-control reference |
| Envelope schematic | [sheet 3](../docs/assets/board/SCH_MOAR_Schematic_3-ENVELOPE_2026-08-13.png) | present | Six-channel envelope-input reference |
| Fabrication directory contents | `hardware/fabrication/README.md` only | boundary note only | States that no orderable fabrication package is enclosed |
| Prototype BOM export | none tracked in `hardware/fabrication/` | absent | No current BOM file is present in this checkout; do not rely on older BOM names |
| Gerber / NC-drill archive | none tracked in `hardware/fabrication/` | absent | No verified fabrication bundle is present or claimed |
| Physical board photos | [top](../docs/assets/board/prodTOP.jpg), [bottom](../docs/assets/board/prodBTM.jpg), [trace](../docs/assets/board/trace.jpg), [bench](../docs/assets/board/bench.jpg), [bring-up](../docs/assets/board/bringup.jpg) | present | Public breadcrumb images for current prototype board review |

## How To Use This Folder

1. Use the three current schematic sheets and linked board photos to confirm the assembled prototype matches the expected wiring and layout.
2. Use [Parts.md](Parts.md) and [Substitutions.md](Substitutions.md) to review parts and likely substitutions before bench work.
3. If you are preparing manufacturing files, stop. This package does not certify a fabrication-ready output set.

## Known Design / Integration Findings

- The physical boards are suitable for probing and bring-up; no fabrication-quality blocker is claimed from the current
  photo and trace-inspection evidence.
- The tracked `hardware/fabrication/` directory is only a status note until verified manufacturing artifacts are added.
- Rail topology and high-current LED behavior still need dated bench evidence before this prototype can be described as
  release-ready.
- Any fault found during bring-up should be logged as a design/integration finding first, then reclassified only if direct
  inspection or measurement points to fabrication.

## What Is Not Verified Yet

- no verified Gerber plus NC-drill release bundle
- no tracked current BOM export in `hardware/fabrication/`
- no tracked fabrication artifact in `hardware/fabrication/` beyond the boundary README
- no claim that any fabrication archive has completed release-level checks
- no order-ready manufacturing sign-off from this package
- no claim that the physical prototype has completed full release-level hardware validation
