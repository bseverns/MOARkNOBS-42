# Hardware Current Build

This page is the docs-site version of the current hardware status summary.

The repo-root canonical source remains `hardware/CurrentBuild.md`. Use that file when you are working from a checkout. This page exists so the MkDocs site can present the same guidance without broken cross-tree links.

## Start Here

1. Check which hardware artifacts are actually present in the repo.
2. Do not assume older BOM or Gerber filenames in older docs are still current.
3. If you need an orderable package, stop unless the canonical hardware page explicitly says the Gerber/NC-drill bundle is verified.

## Current Status Summary

Last mirrored from the canonical hardware page: 2026-07-07.

| Item                      | Version / date cue                                  | Status              | Notes                                                                                                                                  |
| ------------------------- | --------------------------------------------------- | ------------------- | -------------------------------------------------------------------------------------------------------------------------------------- |
| Physical prototype boards | received, photos present under `docs/assets/board/` | `bring-up`          | Boards are in the hardware-test loop; current findings are treated as design/integration issues unless evidence points to fabrication. |
| Schematic reference       | `SCH_MOAR_Schematic_2025-08-30.pdf`                 | `current reference` | Bench-validation reference.                                                                                                            |
| Board drawing reference   | `PCB_MOAR_Board_2025-09-03.pdf`                     | `current reference` | Bench-validation reference.                                                                                                            |
| Fabrication status note   | `hardware/fabrication/README.md`                    | `current boundary`  | States that no orderable fabrication package is enclosed.                                                                              |
| BOM export                | none tracked in `hardware/fabrication/`             | `absent`            | No current BOM file is present in this checkout; do not rely on older BOM names.                                                       |
| Gerber / NC-drill archive | none tracked in `hardware/fabrication/`             | `absent`            | No verified fabrication bundle is present or claimed.                                                                                  |

## Bring-Up Boundary

- The boards are clean enough for useful probing, assembly, and trace inspection.
- Current prototype faults are being treated as design and integration findings unless later measurements identify a fabrication issue.
- The current board, trace-inspection, bench-context, and powered bring-up photos are public breadcrumbs, not release-level validation receipts.
- Rail topology and high-current LED behavior still need dated bench evidence before release-level hardware claims.
- This repo still does not claim an order-ready Gerber plus NC-drill bundle.

## Legacy / stale references called out in the audit

Older docs may mention a hardware-root `.xlsx` BOM, an older fabrication ZIP, or `hardware/shell/`.
Those are not present in the audited checkout and should not be treated as current artifact names.

## Related pages

- [Hardware Substitutions](HardwareSubstitutions.md)
- [Quickstart for Builders](../getting-started/QuickstartForBuilders.md)
- [Repo Health Audit 2026-03](../validation/repo-health-audit-2026-03.md)
