# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- Root README now includes a "Choose Your Path" entry point so prototype validators, firmware contributors, performers, and release-readiness reviewers land on the right docs before diving into the hardware-test package.

### Changed
- Changelog now separates post-`beta0.6.0` documentation edits from the current manifest-backed `dist/` artifact truth.

## [beta0.6.0] - 2026-04-20

### Release/Dist Truth
- `dist/manifest.json` identifies the current package version as `beta0.6.0`, generated on 2026-04-20. It names `mn42_beta0.6.0.hex`, `mn42_beta0.6.0_source.zip`, `hardware_reference.zip`, `THIRD_PARTY_LICENSES.md`, and `release_verification.json`; pair it with `dist/SHA256SUMS.txt` for artifact hashes.
- `dist/release_verification.json` records `optional_skipped_no_port`: `TEST_PORT` was unset, HIL Unity was skipped, and the App, bridge, and full-stack system suites were not run for that package. Do not describe this dist set as HIL/system verified.
- The manifest records branch `main` at commit `5ce01706482a59838e2f1dc390cbdb3b72287577` with `dirty: true` because `.release_verification.json` was untracked at packaging time. Treat the provenance as documented but not a clean tagged release build.
- `dist/hardware_reference.zip` is a reference bundle for review and bench validation, not an order-ready fabrication package. Other `dist/` firmware/source files and `dist/fabrication.zip` are historical or local artifacts unless a current manifest and checksum file name them explicitly.

### Added
- Firmware now exposes real device-backed profile save/load/reset flows plus EEPROM-backed macro snapshot and scene storage, with manifest capability reporting so the App can tell the truth about what the board supports.
- Added stronger storage/regression coverage in the firmware tests plus a destructive `--exercise-storage` lane in `firmware/system_test/mn42_fullstack_runner.js` for bench validation of profile/macro/scene persistence.
- Added `docs/HostCompatibility.md` so browser, bridge, OSC, and DAW support claims are split into verified, documented, and not-claimed buckets instead of being implied from skim-level marketing language.
- WebSerial `GET_MANIFEST` now includes `device_name`, allowing host tools to identify the target rig explicitly instead of inferring identity from version/build data.
- App connect panel now shows a dedicated identity banner (`Connected to: <device> (FW <version>)`) and ships an inline "What to do if connect fails" helper.
- Added firmware panic-safe baseline combo (`Ctrl0 + Ctrl1 + Ctrl2`): stops arp, disables EF follow, and reloads the active profile.
- Added App demo presets `DEMO_A - Reactive Stack` and `DEMO_B - Clock Contrast` for show-ready profile switching.
- Added `docs/DemoPolish.md` runbook covering soak test flow, EXT-clock starvation checks, panic-path verification, and demo asset prep.
- Added Playwright test `App/tests/connection_banner.spec.js` to guard the new connection identity UX in CI.
- Added a persisted Basic/Advanced UI mode in the App (plus `ui_mode` Playwright coverage) so newcomers can stay on plain knob->MIDI mapping while power users keep EF/ARG/filter depth visible.
- Added a guided profile workflow in the App (target slot -> switch/load -> apply -> save), profile import/export staging cues, and tighter profile toolbar behavior around connect/disconnect state.
- Added glossary-style help badges and layman labels across slot editing + schema-rendered forms so EF/ARG/filter/SysEx terms are explained inline instead of buried in docs.
- Added bridge usability docs for every audience level: full operator runbook (`bridge/README.md`), quickstart (`docs/OSCBridge.md`), performer one-pager (`docs/BridgeForPerformers.md`), and packaging plan (`docs/BridgePackaging.md`).
- Added release-prep tooling for bridge packaging: `bridge/scripts/smoke_bridge_cli.js`, `bridge/scripts/package_bridge.sh`, and npm scripts (`smoke`, `package:bridge`, `release:prep`) plus a per-release artifact checklist (`docs/release/bridge-artifacts-checklist.md`).
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
- Scene slot support now ships with named EEPROM-backed saves, `SAVE_SCENE`/`RECALL_SCENE`/`GET_SCENES` verbs, and browser controls that sync scene availability from firmware instead of pretending storage exists.

### Changed
- App/firmware/bridge seams were closed around real production commands instead of simulator-era assumptions: configurator transport now adapts cleanly onto native firmware verbs, bridge host control uses `SET_SLOT_VALUE`, and operator docs were updated to the same contract.
- Browser-only slot notes (`label`, MIDI badge, `Take Control`) now stay strictly browser-local: they no longer travel as device truth, the simulator no longer advertises them as firmware-backed config, and reconnects preserve local notes instead of rehydrating stale defaults from `get_config`.
- Docs now read with a clearer pre-production support boundary: landing pages, connectivity docs, bridge docs, and the repo health audit call out validated host surfaces early, and MkDocs strict build is kept green with the current nav/link structure.
- Connection UI now separates transport state (`Connected`, `Handshaking`, `Disconnected`) from identity details, with device/firmware info moved to the dedicated banner.
- OLED copy pass improves live readability: expanded arp shape names (`Up-Down`, `Random`, `Euclid`), less-abbreviated EF/filter labels, and explicit swing feedback (`Swing: n%`).
- Device Monitor now includes a dedicated `Device` field sourced from manifest identity.
- Bridge and docs indexes now cross-link quickstart/performer/packaging/release-checklist flows, and bridge examples were corrected to the current default OSC ports (`9000` send/listen) so docs match the live bridge behavior.
- `.gitignore` now excludes local hardware CAD project/autosave artifacts (`*.epro`, `*.eprj`, autosave patterns, and `hardware/*_backup/`) so board iteration files can stay local-only while firmware/app/docs ship cleanly.
- Runtime simulator and staged-state handling were tightened for profile/testing flows (`set_param` support, staged/live dirty reconciliation), and profile save now auto-applies dirty staged edits before firmware snapshot commands.
- Added a focused explainability pass across recent architecture hotspots (`App/runtime.js`, `App/views/benzknobz.js`, `App/views/form_renderer.js`, `firmware/src/{Runtime,Scheduler,Protocol,ConfigManager,CommandQueue}.cpp`, `firmware/src/modes/Modes.cpp`) so post-2025 additions read like maintainable design notes instead of archaeology.
- Commit summary (v1.0 prep sweep): newcomer-friendly App UX + profile workflow hardening + bridge usability docs + packaging/release scaffolding + targeted code explainability comments.
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
- Browser-local slot metadata now survives reconnect/profile-refresh paths correctly instead of getting overwritten by live `get_config` payloads during hydrate/replace flows.
- MkDocs strict docs build now passes with the host-compatibility page in nav and the formerly off-nav audit/interop pages explicitly labeled as archive/reference material.
- Connect-failure path now reveals actionable recovery guidance directly in-app instead of leaving users at a dead-end error state.
- Corrected demo-facing OLED abbreviations that were too terse for live explanation of EF/arp/swing states.
- Corrected stale bridge/example docs that still referenced old OSC defaults and legacy bridge verbs; documentation now reflects the current `/mn42/cmd` contract and `--osc/--osc-listen` defaults.
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
