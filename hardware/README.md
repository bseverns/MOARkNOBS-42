# MOARkNOBS-42 Hardware References

This folder contains the hardware references used by the hardware-test package.
Use these files to validate the current prototype on the bench.

Do not treat this folder as a fabrication-ready release bundle.

Start with [CurrentBuild.md](CurrentBuild.md).

## External Power Requirement

Use a regulated `5 V DC` external supply at the DC input.

- `5 V / 4 A` is the minimum recommended bench supply.
- `5 V / 5 A` is preferred for LED-heavy testing (`white100`, `blast`, long soaks).
- Ground must be common across logic, LED, MIDI, and envelope follower sections.

> [!WARNING]
> Do not feed `9 V` or `12 V` into this board's VIN path unless a regulator/buck stage is explicitly added and documented. In this project, `VIN_RAW` and `VIN_FUSED` are project rail labels and should not be interpreted as an Arduino-style high-voltage raw VIN input.

## Power Topology Caveat

The intended fuse topology is:

- preferred: `5V_IN -> F1_LOGIC_0.5A -> 5V_LOGIC`
- preferred: `5V_IN -> F2_LED_2.5A -> 5V_LED`

If the actual board copper instead routes `F2` downstream of `F1`, then `F1` becomes an upstream choke for the entire machine.

> [!WARNING]
> If `F1` is upstream of the LED branch, the `0.5 A` PTC is the effective whole-system current limit. Firmware brightness caps help, but they are not a substitute for correct rail topology and fuse placement.

## Verify Before High-Current LED Tests

Before `white100`, `blast`, or burn-in phases:

- meter DC jack positive to `F1` input
- meter DC jack positive to `F1` output
- meter DC jack positive to `F2` input
- determine whether `F1` and `F2` are parallel branches or whether `F2` is downstream of `F1`
- do not run full-strip white or burn-in tests until this is confirmed

Power estimates and quick current math live in [PowerBudget.md](../docs/reference/PowerBudget.md).

## Firmware Env Boundary

- Flash `teensy40_main` on Rev A or any board whose LED rail topology is still unverified.
- Flash `teensy40_main_reworked` only after the board has been reworked and that split-rail topology has its own dated validation evidence.

## Current Bench-Validation References

- [Power, button, and LED schematic](MN42-machineDrawings/SCH_MOAR_Schematic_1-PWR-BUTTON-LED_2026-08-13.png)
- [Interface, MIDI, and control schematic](MN42-machineDrawings/SCH_MOAR_Schematic_2-INTERFACE-MIDI-CNTRL_2026-08-13.png)
- [Six-channel envelope schematic](MN42-machineDrawings/SCH_MOAR_Schematic_3-ENVELOPE_2026-08-13.png)
- [top-side board photo](../docs/assets/board/prodTOP.jpg) and [bottom-side board photo](../docs/assets/board/prodBTM.jpg)
- [trace-inspection photo](../docs/assets/board/trace.jpg), [bench context](../docs/assets/board/bench.jpg), and [powered bring-up](../docs/assets/board/bringup.jpg)
- [fabrication boundary note](fabrication/README.md)

These files are included to support bench validation and wiring review. No current BOM, Gerber, or NC-drill release bundle is tracked in `fabrication/`.

## Included Notes

- [CurrentBuild.md](CurrentBuild.md)
- [Parts.md](Parts.md)
- [PartsRationale.md](PartsRationale.md)
- [Substitutions.md](Substitutions.md)

## Status Boundary

- current schematic PNG sheets and board photos are present and usable as hardware references
- no current BOM export is tracked in `fabrication/`
- no verified fabrication-ready Gerber and NC-drill bundle is claimed by this package
- the fabrication folder is a boundary note until verified manufacturing files are added
