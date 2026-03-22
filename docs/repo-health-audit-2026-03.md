# Repo Health Audit - 2026-03

This audit was completed from the repository contents on 2026-03-22. It focuses on release clarity, hardware/documentation sync, support readiness, and customer-facing usability.

## Files reviewed

- `README.md`
- `hardware/README.md`
- `hardware/Parts.md`
- `docs/BuildersHandbook.md`
- `docs/ReleaseGuide.md`
- `docs/Troubleshooting.md`
- `docs/WebSerial.md`
- `bridge/README.md`
- `App/README.md`
- `LICENSE`
- `hardware/LICENSE`

## Verified repo facts

- The repo contains hardware reference PDFs in `hardware/MN42-machineDrawings/`:
  - `PCB_MOAR_Board_2025-09-03.pdf`
  - `SCH_MOAR_Schematic_2025-08-30.pdf`
- The repo does not currently contain a versioned BOM spreadsheet or CSV under `hardware/`.
- The repo does not currently contain a versioned Gerber zip under `hardware/fabrication/`.
- The repo does not currently contain the `hardware/shell/` directory referenced by `hardware/README.md`.
- The top-level software license is MIT in `LICENSE`.
- The hardware documentation/design license is CERN OHL v2 Strongly Reciprocal in `hardware/LICENSE`.

## Documentation drift and stale references

### Hardware asset drift

- `hardware/README.md` points builders to `BOM_MOAR_MOAR_Board_2025-08-02.xlsx`, but that file is not present.
- `hardware/README.md` points builders to `fabrication/Gerber_MOAR_Board_2025-08-17.zip`, but that file is not present.
- `hardware/README.md` points to `shell/`, but that directory is not present.
- `hardware/README.md` links to `../docs/README.md#pcb-gallery`, but `docs/README.md` is not present.
- `hardware/README.md` references `Power%3AReg.png`; the repo contains `docs/sketch/Power-Reg.png`.

### Release-truth drift

- `README.md` says CI uploads a fabrication zip during releases, but the repo does not expose a simple, local "current hardware package" page.
- There is no single file that tells a builder which hardware artifacts are current, legacy, experimental, or missing.
- The hardware docs mix "latest" wording with file names that are no longer in the tree.

### Builder and performer onboarding gaps

- A first-time builder has to assemble the workflow from several long-form docs instead of a short procedural entry point.
- A first-time performer has to infer when the browser configurator is enough and when the bridge is required.
- App and bridge docs are detailed, but the "which tool do I need?" question is not answered in one plain-language place.

### Licensing/support clarity gaps

- The MIT software license and CERN OHL hardware license are both present, but the repo does not explain the boundary between them in plain English.
- Non-lawyers are left to infer what "share alike" means for fabricated hardware or sold derivatives.
- Support expectations are implied rather than stated. The docs communicate "as-is" warranty disclaimers, but not a calm support boundary for builders/customers.

## Likely first-time friction points

1. Ordering hardware from stale file references that no longer exist in the checkout.
2. Not knowing whether the machine-drawing PDFs are sufficient to fabricate a board.
3. Not knowing whether to start with the browser configurator or the Node bridge.
4. Not knowing how to do a minimum bring-up after flashing firmware.
5. Not knowing whether substitutes are safe, direct drop-ins, or completely unverified.

## Audit conclusion

The repo already contains strong technical detail, but the current front door is too diffuse for builders and performers, and the hardware asset story is not explicit enough. The immediate fix is documentation, not design churn:

1. Add one canonical hardware status page.
2. Add short builder/performer quickstarts.
3. Add a plain-language connectivity guide.
4. Add a plain-language license/support boundary doc.
5. Repoint README surfaces to those pages and remove stale "latest" claims that are not supported by the current tree.
