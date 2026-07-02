# Current Hardware Build

This is the hardware status page for the hardware-test package.

Last audited against repo contents: 2026-07-02.

## Package Scope

This package supports prototype bench validation.
It does not claim fabrication readiness.

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

Remaining public-photo needs are tracked in `docs/project/TODO.md`, not treated as hardware evidence.

## Current Hardware References

| Item | Path | Status | Use in this package |
| --- | --- | --- | --- |
| Schematic reference | `hardware/MN42-machineDrawings/SCH_MOAR_Schematic_2025-08-30.pdf` | present | Current schematic reference for bench validation |
| PCB/reference drawing | `hardware/MN42-machineDrawings/PCB_MOAR_Board_2025-09-03.pdf` | present | Current board drawing reference for bench validation |
| Fabrication status note | `hardware/fabrication/README.md` | present | States that no orderable fabrication package is enclosed |
| Prototype BOM export | none tracked in `hardware/fabrication/` | absent | No current BOM file is present in this checkout; do not rely on older BOM names |
| Gerber / NC-drill archive | none tracked in `hardware/fabrication/` | absent | No verified fabrication bundle is present or claimed |
| Physical board photos | `docs/assets/board/` | present | Public breadcrumb images for current prototype board review |

## How To Use This Folder

1. Use the schematic PDF and PCB/reference drawing PDF to confirm the assembled prototype matches the expected wiring and layout.
2. Use [Parts.md](Parts.md) and [Substitutions.md](Substitutions.md) to review parts and likely substitutions before bench work.
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
- no tracked current BOM export in `hardware/fabrication/`
- no claim that any fabrication archive has completed release-level checks
- no order-ready manufacturing sign-off from this package
- no claim that the physical prototype has completed full release-level hardware validation
