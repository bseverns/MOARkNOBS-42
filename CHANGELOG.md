# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- Nothing stamped yet. We’re still catching our breath after the `v0.2.0` drop and taking notes for the next riff.

### Fixed
- Bulk CC dumps finally stash channels under `EEPROM_POT_CHANNELS` and park CC numbers under `EEPROM_POT_CC`, so `Utility::processBulkUpdate`
  lines up with the documented EEPROM map instead of gaslighting ConfigManager.

## [0.2.0] - 2025-10-31

### Added
- Demo USB MIDI sketch under `firmware/test/demo_button_ef_usb_midi.cpp` shows a control button punching out note-on/off while the envelope follower breathes CC data back to the host DAW.
- Unity test docs flag the new demo so folks know where to start when sniffing the USB transport without spelunking the test harness.
- Diagnostic mode boots with self-test pages and a compact matrix view so field checks stop feeling like guesswork.
- Boot banner now decodes reset causes and spits a system report before the synth even clears its throat.
- Release pipeline and a pre-commit lint job keep CI honest and reproducible.
- Filter tuning pots let you twist the analog guts without reaching for a screwdriver mid-jam.
- Arpeggiator learned to wiggle on its own—Perlin noise jitters included.
- WebSerial telemetry spews live state into the browser, because the best debugging tools are the ones you already have open.
- NRPN/RPN/SysEx support unlocks MIDI’s weird alleys instead of sticking to vanilla CCs.
- Start tracking changes with this log so the paper trail matches the noise we’re making.
- Pin map and EEPROM layout docs locked down for builders who like schematics with their solder fumes.
- SparkFun reference sidebars point straight to the solder-stained source material.
- Web configurator now drives all slot parameters (EF, ARG, LED colour, full MIDI types) and falls back to a bundled schema when the device won’t deliver one.
- `docs/HISTORY.md` now carries a “how to read this thing” primer so future builders can trace the arc without spelunking the git log first.
- Added a September status rollup capturing the post-`v0.1.0` cleanup sprint and what we’re staging for the next hardware spin.
- Dropped breadcrumbs to the bridge scripts so folks can sling telemetry without reverse engineering the tooling.

### Changed
- Bridge rides Node 20 and clang-format marches in lockstep with CI.
- README now calls out firmware build steps and links the new docs.
- HISTORY timeline is chunked by focus blocks instead of loose commit lists—more story, less archeology.
- ConfigManager now coughs up the MIDI channel map it stashes in EEPROM so fresh boots inherit the saved routing without extra glue code.
- PotentiometerManager and Globals keep their EEPROM offsets honest, carving out extra JSON buffer slack so oversized hardware presets don’t brick the load.
- Unity system report test pipes its output through `PrintTarget` and `Serial1`, matching the custom transport instead of whispering into the void.

### Removed
- OctoWS2811 vendored library; FastLED handles the LEDs solo now.
- Blocked the Teensy core’s OctoWS2811 copy via `lib_ignore` to keep builds clean.

### Fixed
- Pot channel persistence handoff won’t drop stored assignments when ConfigManager rebuilds the control surface on boot.

## [0.1.0] - 2025-08-06
### Added
- First public release of MOARkNOBS-42.

[0.2.0]: https://github.com/bseverns/MOARkNOBS-42/releases/tag/v0.2.0
[0.1.0]: https://github.com/bseverns/MOARkNOBS-42/releases/tag/v0.1.0
