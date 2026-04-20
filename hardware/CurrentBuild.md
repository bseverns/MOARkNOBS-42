# Current Hardware Build

This is the canonical hardware entry point for the repository. Check this page before ordering boards or parts.

Last audited against repo contents: 2026-03-22.

## Start Here

1. Read this page first.
2. If you need the latest verified files that are actually present in the repo, use the machine-drawing PDFs listed below.
3. If you need a release-ready fabrication package, stop and check the status table in this file before placing an order.

## Status Labels

- `current` means the file is present in the repo and is the best verified reference available in this checkout.
- `legacy` means the file name still appears in docs, but the file is not present in this checkout.
- `experimental` means the repo does not provide enough evidence to call it release-ready.

## Current Status Table

| Item | Path | Version / date cue | Status | Notes |
| --- | --- | --- | --- | --- |
| Board drawing reference | `hardware/MN42-machineDrawings/PCB_MOAR_Board_2025-09-03.pdf` | `2025-09-03` from filename | `current` | Verified present in repo. Use as a drawing/reference artifact, not as a substitute for a checked fabrication archive. |
| Schematic reference | `hardware/MN42-machineDrawings/SCH_MOAR_Schematic_2025-08-30.pdf` | `2025-08-30` from filename | `current` | Verified present in repo. |
| Fabrication zip | `hardware/fabrication/` | none present | `experimental` | `TODO: add a versioned fabrication archive under hardware/fabrication/ and update this table with the exact filename.` |
| BOM export | `hardware/fabrication/BOM_MOAR_MOAR_Board_2026-03-17.xlsx` | `2026-03-17` from filename | `current` | Prototype BOM present in repo as an `.xlsx` export. Confirm it matches the intended board revision before ordering. |

## What Builders Should Download First

If you are evaluating the current repo state today:

1. Read `hardware/CurrentBuild.md`.
2. Download or open the board drawing PDF and schematic PDF listed in the table above.
3. Do not assume older BOM/Gerber filenames in docs are current.
4. If you need an orderable package, use the BOM listed above and confirm whether a release-ready fabrication archive exists before spending money.
