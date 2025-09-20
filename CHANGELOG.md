# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- Diagnostic mode boots with self-test pages and a compact matrix view.
- Boot banner now decodes reset causes and spits a system report.
- Release pipeline and a pre-commit lint job keep CI honest.
- Filter tuning pots let you twist the analog guts without a screwdriver.
- Arpeggiator learned to wiggle on its own, Perlin noise and all.
- WebSerial telemetry spews live state into the browser.
- NRPN/RPN/SysEx support because MIDI's dark corners are fun.
- Start tracking changes with this log.
- Pin map and EEPROM layout docs locked down.
- SparkFun reference sidebars point straight to the solder-stained source.
- Web configurator now drives all slot parameters (EF, ARG, LED colour, full MIDI types) and falls back to a bundled schema when the device won't deliver one.
- `docs/HISTORY.md` now carries a "how to read this thing" primer so future builders can trace the arc without spelunking the git log first.
- Added a September status rollup capturing the post-`v0.1.0` cleanup sprint and what we're staging for the next hardware spin.
- Dropped breadcrumbs to the bridge scripts so folks can sling telemetry without reverse engineering the tooling.

### Changed
- Bridge rides Node 20 and clang-format marches in lockstep with CI.
- README now calls out firmware build steps and links the new docs.
- HISTORY timeline is chunked by focus blocks instead of loose commit lists—more story, less archeology.

### Removed
- OctoWS2811 vendored library; FastLED handles the LEDs solo now.
- Blocked the Teensy core's OctoWS2811 copy via `lib_ignore` to keep builds clean.

## [0.1.0] - 2025-08-06
### Added
- First public release of MOARkNOBS-42.

[0.1.0]: https://github.com/bseverns/MOARkNOBS-42/releases/tag/v0.1.0
