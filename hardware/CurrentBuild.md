# Current Hardware Build

This is the hardware status page for the hardware-test package.

Last audited against repo contents: 2026-04-21.

## Package Scope

This package supports prototype bench validation.
It does not claim fabrication readiness.

## Current Hardware References

| Item | Path | Status | Use in this package |
| --- | --- | --- | --- |
| Schematic reference | `hardware/MN42-machineDrawings/SCH_MOAR_Schematic_2025-08-30.pdf` | present | Current schematic reference for bench validation |
| PCB/reference drawing | `hardware/MN42-machineDrawings/PCB_MOAR_Board_2025-09-03.pdf` | present | Current board drawing reference for bench validation |
| Prototype BOM export | `hardware/fabrication/BOM_MOAR_MOAR_Board_2026-03-17.csv` | present | Current BOM export for prototype parts review |
| Gerber archive | `hardware/fabrication/Gerber_MOAR_Board_1_2026-02-24.zip` | present but unverified | Review-only artifact; not claimed as a verified fabrication bundle |

## How To Use This Folder

1. Use the schematic PDF and PCB/reference drawing PDF to confirm the assembled prototype matches the expected wiring and layout.
2. Use the BOM export, [Parts.md](Parts.md), and [Substitutions.md](Substitutions.md) to review parts and likely substitutions before bench work.
3. If you are preparing manufacturing files, stop. This package does not certify a fabrication-ready output set.

## What Is Not Verified Yet

- no verified Gerber plus NC-drill release bundle
- no claim that the current fabrication archive has completed release-level checks
- no order-ready manufacturing sign-off from this package
