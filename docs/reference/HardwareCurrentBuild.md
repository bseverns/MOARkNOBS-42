# Hardware Current Build

This page is the docs-site version of the current hardware status summary.

The repo-root canonical source remains `hardware/CurrentBuild.md`. Use that file when you are working from a checkout. This page exists so the MkDocs site can present the same guidance without broken cross-tree links.

## Start Here

1. Check which hardware artifacts are actually present in the repo.
2. Do not assume older BOM or Gerber filenames in older docs are still current.
3. If you need an orderable package, stop unless the canonical hardware page explicitly says the Gerber/NC-drill bundle is verified.

## Current Status Summary

Last mirrored from the canonical hardware page: 2026-06-28.

| Item                      | Version / date cue                                  | Status                   | Notes                                                                                                                                  |
| ------------------------- | --------------------------------------------------- | ------------------------ | -------------------------------------------------------------------------------------------------------------------------------------- |
| Physical prototype boards | received, photos present under `docs/assets/board/` | `bring-up`               | Boards are in the hardware-test loop; current findings are treated as design/integration issues unless evidence points to fabrication. |
| Schematic reference       | `SCH_MOAR_Schematic_2025-08-30.pdf`                 | `current reference`      | Bench-validation reference.                                                                                                            |
| Board drawing reference   | `PCB_MOAR_Board_2025-09-03.pdf`                     | `current reference`      | Bench-validation reference.                                                                                                            |
| BOM export                | `BOM_MOAR_MOAR_Board_2026-03-17.csv`                | `current prototype BOM`  | Present under `hardware/fabrication/` as the current prototype parts-review export.                                                    |
| Gerber archive            | `Gerber_MOAR_Board_1_2026-02-24.zip`                | `present but unverified` | Review-only artifact; not a verified fabrication release bundle.                                                                       |

## Bring-Up Boundary

- The boards are clean enough for useful probing, assembly, and trace inspection.
- Current prototype faults are being treated as design and integration findings unless later measurements identify a fabrication issue.
- Rail topology and high-current LED behavior still need dated bench evidence before release-level hardware claims.
- This repo still does not claim an order-ready Gerber plus NC-drill bundle.

## Legacy / stale references called out in the audit

- `hardware/BOM_MOAR_MOAR_Board_2025-08-02.xlsx`
- `hardware/fabrication/Gerber_MOAR_Board_2025-08-17.zip`
- `hardware/shell/`

Those names were referenced by older docs but were not present in the audited checkout.

## Related pages

- [Hardware Substitutions](HardwareSubstitutions.md)
- [Quickstart for Builders](../getting-started/QuickstartForBuilders.md)
- [Repo Health Audit 2026-03](../validation/repo-health-audit-2026-03.md)
