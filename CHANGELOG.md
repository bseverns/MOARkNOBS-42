# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- Filter tuning pots let you twist the analog guts without a screwdriver.
- Arpeggiator learned to wiggle on its own, Perlin noise and all.
- WebSerial telemetry spews live state into the browser.
- NRPN/RPN/SysEx support because MIDI's dark corners are fun.
- Start tracking changes with this log.
- Pin map and EEPROM layout docs locked down.

### Changed
- README now calls out firmware build steps and links the new docs.

### Removed
- OctoWS2811 vendored library; FastLED handles the LEDs solo now.
- Blocked the Teensy core's OctoWS2811 copy via `lib_ignore` to keep builds clean.

## [0.1.0] - TBD
### Added
- First public release of MOARkNOBS-42.

[0.1.0]: https://github.com/bseverns/MOARkNOBS-42/releases/tag/v0.1.0
