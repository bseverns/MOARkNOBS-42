# Hardware Current Build

This page is the docs-site version of the current hardware status summary.

The repo-root canonical source remains `hardware/CurrentBuild.md`. Use that file when you are working from a checkout. This page exists so the MkDocs site can present the same guidance without broken cross-tree links.

## Start Here

1. Check which hardware artifacts are actually present in the repo.
2. Do not assume older BOM or Gerber filenames in older docs are still current.
3. If you need an orderable package, stop unless the canonical hardware page explicitly says the Gerber/NC-drill bundle is verified.

## Current Status Summary

Last mirrored from the canonical hardware page: 2026-08-13.

| Item                      | Version / date cue                                    | Status               | Notes                                                                                                                                  |
| ------------------------- | ----------------------------------------------------- | -------------------- | -------------------------------------------------------------------------------------------------------------------------------------- |
| Physical prototype boards | received, photos present under `docs/assets/board/`   | `bring-up`           | Boards are in the hardware-test loop; current findings are treated as design/integration issues unless evidence points to fabrication. |
| Power/button/LED schematic | [sheet 1](../../hardware/MN42-machineDrawings/SCH_MOAR_Schematic_1-PWR-BUTTON-LED_2026-08-13.png) | `current reference` | Power, button-matrix, and LED bench reference. |
| Interface/MIDI/control schematic | [sheet 2](../../hardware/MN42-machineDrawings/SCH_MOAR_Schematic_2-INTERFACE-MIDI-CNTRL_2026-08-13.png) | `current reference` | Teensy interface, MIDI, OLED, and direct-control reference. |
| Envelope schematic | [sheet 3](../../hardware/MN42-machineDrawings/SCH_MOAR_Schematic_3-ENVELOPE_2026-08-13.png) | `current reference` | Six-channel envelope-input reference. |
| Fabrication directory     | tracked contents are `hardware/fabrication/README.md` | `boundary note only` | States that no orderable fabrication package is enclosed.                                                                              |
| BOM export                | none tracked in `hardware/fabrication/`               | `absent`             | No current BOM file is present in this checkout; do not rely on older BOM names.                                                       |
| Gerber / NC-drill archive | none tracked in `hardware/fabrication/`               | `absent`             | No verified fabrication bundle is present or claimed.                                                                                  |

## Bring-Up Boundary

- The boards are clean enough for useful probing, assembly, and trace inspection.
- Current prototype faults are being treated as design and integration findings unless later measurements identify a fabrication issue.
- The current board, trace-inspection, bench-context, and powered bring-up photos are public breadcrumbs, not release-level validation receipts.
- `hardware/fabrication/` is a boundary-note directory only until verified manufacturing artifacts are added.
- Rail topology and high-current LED behavior still need dated bench evidence before release-level hardware claims.
- This repo still does not claim an order-ready Gerber plus NC-drill bundle.

## Legacy / stale references called out in the audit

Older docs may mention a hardware-root `.xlsx` BOM, an older fabrication ZIP, or `hardware/shell/`.
Those are not present in the audited checkout and should not be treated as current artifact names.

## Related pages

- [Hardware Substitutions](HardwareSubstitutions.md)
- [Quickstart for Builders](../getting-started/QuickstartForBuilders.md)
- [Repo Health Audit 2026-03](../validation/repo-health-audit-2026-03.md)
