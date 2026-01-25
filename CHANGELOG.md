# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- Playwright simulator rig bolts into CI and `test.sh`, so you can hammer the
  App’s UI without cracking open the firmware—kick it off locally with
  `npm --prefix App test` when you want browser-level truth before wiring up
  the hardware. The suite now rewrites the manifest mid-run to trigger the
  migration dialog, rehearses rollback, and proves a clean apply clears the diff
  badge.
- Hardware-in-the-loop system runner (`firmware/system_test/mn42_fullstack_runner.js`)
  now ships as a first-class script: it spawns the bridge, drives the OSC slot
  patch scenarios called out in the docs, and emits JSON + text logs ready for
  CI artifacts or late-night lab notes.
- Slot ownership got formalized: each slot now names its owning manager and
  persistence block, with the EEPROM serializer carving out explicit 64-byte
  pages so firmware, WebSerial, and Unity tests stop stepping on each other's
  toes when they rebuild schema drafts.
- Slot configs now stash 16-byte SysEx templates with `XX`/`MSB`/`LSB` placeholders, and the WebSerial editor lets you script
  those bursts without cracking open the firmware.
- Per-slot envelope follower settings (filter type, cutoff, Q) live alongside each MIDISlot and persist to EEPROM, bumping the
  schema to `0x0004` so stored patches remember their curves.
- Dual LFO engines now live in `firmware_main.cpp` via `LFOManager`, routing LED brightness, arp swing, and EF gain trim while persisting shapes/routes inside the profile payload so both UART telemetry and WebSerial keep the same modulation state. Unity LFO tests guard the shapes and clock sync math. [2fe3c15]
- The Web Configurator was rebuilt around a split runtime kernel and BenzKnobz view layer: schema-driven forms, staged diff/rollback controls, telemetry cards, a MIDI monitor, and a simulator toggle all live in `runtime.js`/`views/*` plus new Playwright specs that exercise every panel without a human in Chromium. [2fe3c15]
- Serial commands now route through a sorted dispatch table with standalone handler functions, an exposed `dispatchCommand` for Unity, and an "unknown command" log that also prints the available verbs; new Unity tests hit both a known and unknown command so the glue layer stays covered. [current change]
- Added `test/test_protocol_dispatch.cpp` and the `firmware/python` shim that makes `python -c ...` resolve to `python3`, keeping the Unity runner happy in environments without a bare `python`. [current change]
- Macro snapshot controls now expose `SAVE_MACRO_SLOT`/`RECALL_MACRO_SLOT` to the WebSerial UI with inline feedback, disabled save during writes, and optional config reloads after recall so the browser mirrors slot 254 without touching live profiles.
- Introduced LED animation modes (`Static`, `PeakHold`, `Trail`, `ClockPulse`) plus a scheduler-driven `LedAnimator`, diagnostics override, and RPC hooks so pots/envelopes can emit dynamic feedback safely outside interrupt context.
- Planned scene slot support: fixed-size `Scene` records with named slots, `SAVE_SCENE`/`RECALL_SCENE` verbs, and a future WebSerial “Scenes” view that lists saved names and manages EEPROM-backed snapshots without blocking the loop.

### Changed
- Runtime now runs a normalization/sanitization lap that clamps slot payloads
  before validation/apply, so the control surface stops seeing ghost values
  when the `runtime` bridge chews on sketchy input.
- Firmware source files now carry teaching-forward comment blocks explaining
  data flow, pointer ownership, and math choices so workshops can read the code
  like a zine instead of a riddle.
- The November comment pass now ships with subject headers—boot choreography,
  input scanning, persistence hygiene, and signal-flow math—so instructors can
  stitch the annotated files into lesson plans without reverse engineering the
  comment trail.
- Root and firmware READMEs map the control stack with new "crash course" and
  "stack philosophy" sections, pointing students toward the annotated modules
  and the docs they feed.
- Testing docs call their shot: `docs/TESTING.md` now walks through the
  full-stack runner flags so contributors can actually reproduce the OSC↔firmware
  handshake the release notes keep hyping.
- Added an "Annotated Source Field Guide" and guided-tour curriculum so mentors
  know exactly which `.cpp` to open when explaining globals, mux math, or MIDI
  queueing.

### Fixed
- Status/diff badges finally clock the `hidden` flag and swap styles instead of
  pretending they’re always visible—badge CSS tweaks landed in the App so the
  UI doesn’t gaslight folks skimming change reports.
- Bulk CC dumps finally stash channels under `EEPROM_POT_CHANNELS` and park CC numbers under `EEPROM_POT_CC`, so `Utility::processBulkUpdate`
  lines up with the documented EEPROM map instead of gaslighting ConfigManager.
- Incoming SysEx packets longer than 64 bytes now get dropped at the door unless you explicitly forward them, keeping rogue gear
  from hogging RAM.

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
- OctoWS2811 DMA driver is back under `firmware/lib/OctoWS2811/` so Teensy 4 builds keep upstream timing guarantees even when we compile offline.

### Removed
- OctoWS2811 vendored library; FastLED handles the LEDs solo now. *(Undone by the October 2025 DMA revival noted above.)*
- Blocked the Teensy core’s OctoWS2811 copy via `lib_ignore` to keep builds clean.

### Fixed
- Pot channel persistence handoff won’t drop stored assignments when ConfigManager rebuilds the control surface on boot.
- Bumping the EEPROM schema to `0x0003` wipes and realigns the slot arena before wider SysEx-ready structs load, so upgrades stop trampling profile blocks.

## [0.1.0] - 2025-08-06
### Added
- First public release of MOARkNOBS-42.

[0.2.0]: https://github.com/bseverns/MOARkNOBS-42/releases/tag/v0.2.0
[0.1.0]: https://github.com/bseverns/MOARkNOBS-42/releases/tag/v0.1.0
