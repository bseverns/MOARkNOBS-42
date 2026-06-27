# MOARkNOBS-42 Hardware-Test Package

This repository is currently packaged as a hardware-test bundle for the MOARkNOBS-42 prototype.
It is meant for bench validation of the current board, firmware, OLED, controls, MIDI paths,
LEDs, envelope followers, WebSerial telemetry, and bridge/configurator connectivity.

It is not a public v1.0 release.
It is not a fabrication-ready manufacturing package.
It does not claim a verified Gerber and NC-drill release bundle.

Start with [HARDWARE_TEST_README.md](HARDWARE_TEST_README.md).
For a repo-level contents map, see [docs/project/RepositoryContents.md](docs/project/RepositoryContents.md).

## PCB Progress Snapshot

The prototype PCB run has moved from design files into physical boards that are now part of the hardware-test loop.
The current goal is bring-up evidence: power-rail checks, display and control validation, MIDI path testing, envelope follower
behavior, LED load testing, and host connectivity through the WebSerial app and bridge.

This is the first public breadcrumb for the fabricated boards, not a release claim. The boards are being inspected and
validated against the hardware references below, and the repo will keep fabrication packages clearly labeled until a
revision has completed release-level checks.

So far, the physical board quality has been good enough to make the prototype review useful: the PCB itself gives a
stable target for probing, assembly, and trace inspection, and the issues found during bring-up appear to be design
iteration problems in this machine rather than fabrication-quality problems with the boards.

![Top side of the MOARkNOBS-42 prototype PCB from the recent fabrication run.](docs/assets/board/prodTOP.jpg)

![Bottom side of the MOARkNOBS-42 prototype PCB from the recent fabrication run.](docs/assets/board/prodBTM.jpg)

![Close-up trace inspection photo from the MOARkNOBS-42 prototype PCB bring-up work.](docs/assets/board/trace.jpg)

Photo TODOs before the full project post:

- TODO image: assembled front panel with knobs, buttons, OLED, and LEDs installed
- TODO image: bench setup during first-power and rail-topology validation
- TODO image: MIDI, USB, and WebSerial/bridge test session with the board connected
- TODO image: any required rework close-up, or a clear "no rework required" board close-up after validation

## PCB Fabrication Notes

- Existing photos live under [docs/assets/board](docs/assets/board).
- Current hardware status lives in [hardware/CurrentBuild.md](hardware/CurrentBuild.md).
- Fabrication caveats live in [hardware/fabrication/README.md](hardware/fabrication/README.md).
- Fabhouse experience note: the boards arrived cleanly enough that the current prototype faults are being treated as
  my own design and integration findings unless future evidence says otherwise. That is exactly what this prototype
  run needed to answer.

## Choose Your Path

- **Validating prototype hardware:** start with [HARDWARE_TEST_README.md](HARDWARE_TEST_README.md), then follow
  [docs/hardware-test/Bringup.md](docs/hardware-test/Bringup.md) and
  [docs/hardware-test/TestMatrix.md](docs/hardware-test/TestMatrix.md).
- **Changing firmware:** use [firmware/README.md](firmware/README.md) for PlatformIO lanes and
  [CONTRIBUTING.md](CONTRIBUTING.md) for the build/test contract.
- **Using the controller musically:** start with
  [docs/getting-started/QuickstartForPerformers.md](docs/getting-started/QuickstartForPerformers.md), then use
  [docs/getting-started/GuidedRoutes.md](docs/getting-started/GuidedRoutes.md) when you want the broader docs map.
- **Checking fabrication or release readiness:** read [hardware/CurrentBuild.md](hardware/CurrentBuild.md) and keep the
  non-release caveats above in mind before treating any hardware artifacts as orderable.

## Package Entry Points

- [HARDWARE_TEST_README.md](HARDWARE_TEST_README.md) explains what the package is, what it is not, and the expected bring-up flow.
- [docs/hardware-test/Bringup.md](docs/hardware-test/Bringup.md) is the first-boot checklist.
- [docs/hardware-test/TestMatrix.md](docs/hardware-test/TestMatrix.md) lists the supported test lanes and commands.
- [docs/hardware-test/KnownIssues.md](docs/hardware-test/KnownIssues.md) records current package limits.
- [docs/hardware-test/PackageManifest.md](docs/hardware-test/PackageManifest.md) lists included and excluded lanes.
- [hardware/CurrentBuild.md](hardware/CurrentBuild.md) identifies the current hardware reference files.
- [docs/project/RepositoryContents.md](docs/project/RepositoryContents.md) explains which repo areas are source truth, evidence, generated output, or archive.

## Included Test Surfaces

- `firmware/` for the Teensy 4.0 firmware and test environments
- `hardware/` for the current schematic, PCB drawing, BOM, and hardware notes
- `App/` for WebSerial configurator validation
- `bridge/` for OSC and virtual MIDI bridge validation
- `tools/` for link checks, bench capture, and package export

## Primary Build Command

Run PlatformIO from `firmware/`, not the repo root:

```bash
pio run -d firmware -e teensy40_main
```

## Hardware Reference Files Present

- Schematic PDF: `hardware/MN42-machineDrawings/SCH_MOAR_Schematic_2025-08-30.pdf`
- PCB/reference drawing PDF: `hardware/MN42-machineDrawings/PCB_MOAR_Board_2025-09-03.pdf`
- Prototype BOM export: `hardware/fabrication/BOM_MOAR_MOAR_Board_2026-03-17.csv`

Use those as bench-validation references only. See [hardware/CurrentBuild.md](hardware/CurrentBuild.md)
for the current status notes.
