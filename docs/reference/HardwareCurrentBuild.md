# Hardware Current Build

This page is the docs-site version of the current hardware status summary.

The repo-root canonical source remains `hardware/CurrentBuild.md`. Use that file when you are working from a checkout. This page exists so the MkDocs site can present the same guidance without broken cross-tree links.

## Start Here

1. Check which hardware artifacts are actually present in the repo.
2. Do not assume older BOM or Gerber filenames in older docs are still current.
3. If you need an orderable package, confirm that the fabrication archive and BOM are explicitly listed as present.

## Current Status Summary

| Item                      | Version / date cue                    | Status         | Notes                                                                      |
| ------------------------- | ------------------------------------- | -------------- | -------------------------------------------------------------------------- |
| Board drawing reference   | `PCB_MOAR_Board_2025-09-03.pdf`       | `current`      | Verified present in the repo audit.                                        |
| Firmware hex              | `dist/mn42_{VERSION}.hex`             | `production`   | Requires Teensy Loader or `teensy_loader_cli`.                             |
| Hardware reference bundle | none present in audited tree          | `experimental` | `TODO: add a versioned fabrication archive under hardware/fabrication/.`   |
| BOM export                | `BOM_MOAR_MOAR_Board_2026-03-17.xlsx` | `current`      | Present under `hardware/fabrication/` as the current prototype BOM export. |

## Legacy / stale references called out in the audit

- `hardware/BOM_MOAR_MOAR_Board_2025-08-02.xlsx`
- `hardware/fabrication/Gerber_MOAR_Board_2025-08-17.zip`
- `hardware/shell/`

Those names were referenced by older docs but were not present in the audited checkout.

## Related pages

- [Hardware Substitutions](HardwareSubstitutions.md)
- [Quickstart for Builders](../getting-started/QuickstartForBuilders.md)
- [Repo Health Audit 2026-03](../validation/repo-health-audit-2026-03.md)
