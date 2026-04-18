# Project History

This file summarizes the development of the MOARkNOBS-42 project based on commit history with a few inserts re: design choices

## How to Read this History

- **Start with the focus tags.** Each block pairs a time range with the main obsession so you know whether you're about to read about PCB copper or MIDI voodoo.
- **Skim the italic callouts** for the "why" behind the commits—they're the diary fragments that explain decisions better than dry hashes ever could.
- **Use the bracketed references** to chase the actual commits or tags if you want to see the gritty diffs.
- **Bridge notes** flag the `bridge/` tooling so you can wire telemetry without cobbling your own serial hacks.

## 2024

- _I decided I wanted to build more instruments and wanted to explore MIDI and microcontrollers more expansively. Heavy early research around simple machines, 1-function mock ups, and C++ best-practices began. Initial functional inspiration: Bastl Instruments '60 Knobs' ([repo](https://github.com/bastl-instruments/60knobs)) and their wild [experimental playground](https://github.com/bastl-instruments/60knobs-experimental)_
- **December 10:** Initial commit introduces the firmware source tree with modules for button scanning, MIDI handling, display control, and EEPROM support. [58ef040]
- **Mid December:** Early work on display management and multiplexed button matrix ("mux management" and "display/managers").
- _More complicated in areas that I didn't anticipate complexity in than in the outright complicated parts_
- **Late December:** Adds EEPROM and USB MIDI functionality, debounce logic, and second revision of the display code. [dbf3c21, 788a1ba]

## 2025

- _How is this thing supposed to happen, as more than just a simple experiment?_

### January – March — Hardware Layout Bootstrapping

- Hardware design begins in KiCad. Multiple board iterations are committed along with early firmware clean‑ups and configuration handling. [70665ff, 4c22a8b]
- _My old workstation only runs up to kiCAD 6_
- By late January the "easyeda" project is created and the firmware tree moves under `firmware/` with substantial PCB files added. [20e518a]
- February and March see continual board revisions (`brd`, `brd v2`), button routing tweaks, and resistor pull-up improvements. [f36d220, dd0ebb6]

### April — Repo Reorg & First Board Spin

- Repository reorganization and creation of the first official PCB revision `MN42-1` in EasyEDA. [174b516, f5916a7]
- _Work just a little at a time, and eventually you have a whole castle_

### May — Firmware Refine & Test Harness

- Firmware refactors focus on display timing, envelope follower behavior, and general stability. README updates document usage. [44e89dc, ec7edb3]
- Initial testing framework appears along with incremental fixes. [ed357da, 6a4e54a]
- _Getting onto breadboards for timing testing/display/midi/etc._

### June — Filters, MIDI Depth, & Documentation

- Early June introduces unit tests for the BiquadFilter and bug fixes around fade animations. [b050f4f, 69fd0db]
- **June 17:** Tagged as `firmware1.0`, marking a stable baseline before adding more features. [04f6b72]
- _I bet this can do so much more than CC really easy_
- Following this, MIDI capabilities expand significantly with new message types and slot verification. [6cef527, 0e5c9d9]
- **June 23:** MIDI type combos and filter tuning feedback are implemented, and documentation is expanded. [2615616, ab8078a]
- _Decided to give AI tools a try because who am I to say no to a perfectly good tool?_
- Extensive cleanup on June 25 consolidates pin assignments, removes old files, adds the MIT license, and updates hardware docs. [bfbfbaf, cb39705]
- Additional DSP tests and control button wiring verification land on June 26. [b703f1f, 1a2b239]
- Late June brings documentation improvements describing envelope filters, ARG mode, and exhaustive testing approaches. [43b1d33, b14ef44]
- **June 28:** Final `readme tweaks` commit wraps up the documented state of the project. [8ed5568]
- _^^^ see what I mean? I am tyring to get better at documentation as I go, but this helps explain what is happening so much more than my drafts alone. I just have an assistant now._

### July — Arpeggiator Expansion & Testing Discipline

- Arpeggiator mode added with base note support and full MIDI type coverage. [850b4e9, 60abd55, e03466d]
- PlatformIO build updated to include the arpeggiator sources. [6b81ed0, 1e9a015]
- README explains new arpeggiator behavior. [09ffda2]
- Test suite expanded: biquad filter modes and EEPROM slot verification. [2716465, 9a8733b]
- Button-driven navigation simplifies test phases. [8716674]
- Helper functions unify test initialization. [bf1936f, 109a1b8]
- Variables clarified for easier maintenance. [6d769ec]
- Documentation overhaul adds flashing instructions, diagrams, and links back to this history. Firmware comments and tests gain more clarity. [822a1ea, 5f3e34b, e25622e, 0941fcb]

### August — Release, CI Discipline, and Telemetry

- Finalizing helpers and test suites.
- Hardware v1.02 design completed.
- Clarifications added to language and comments expanded throughout the repo.
- Refactored `Utility::processBulkUpdate` in `firmware/src/Utility.cpp` to chew through bulk updates with a raw char buffer.
- Dropped the gritty `MN42_v2` hardware rev with fresh design assets under `hardware/MN42-1/`.
- `fea338e` calls for thicker LED power traces in `hardware/MN42-1/`—beefier copper keeps the LEDs from sagging or cooking when you crank the current.
- NRPN, RPN, and raw SysEx support crash the party, ditching vanilla CCs for full‑fat MIDI mojo and teaching us how deep the protocol rabbit hole really goes.
- WebSerial begins streaming the synth's guts straight to the browser; the editor's rough edges prove the web can be a lab bench if you don't mind a little chaos.
- _If you ever wanted a synth to show you its source and its soul, this is the moment._
- Wired up the filter‑tuning pot and roughed up the arpeggiator—hands‑on analog control meets sequencer swagger, plus a reminder that drift and off‑by‑ones are always lurking.
- **August 6:** First public drop lands as `v0.1.0`, bundling filter‑tuning pots, a self‑driving arpeggiator, WebSerial telemetry, NRPN/RPN/SysEx support, and the project's inaugural `CHANGELOG`. [v0.1.0]
- **August 8:** Merged PR #286, hauling in the full FastLED arsenal and scribbling teachable comments all over `firmware/App/benzknobz.html`. [956069c]
- **August 11** My ears are still ringing because I went to a metal show in the basement of an American Legion. I am scrambling to make these Unity testers work for the codebase as well as actual builds of the firmware.
- _Standards are not optional; often the best lessons come from coloring off the page._
- **Late August:** CI kept flaking out, so the build scripts took a beating. Swapped the MIDI library, chased phantom `usb_midi` ghosts, and bolted on a Unity test rig so every commit has to prove itself.
- **August 22–23:** Diagnostic mode lands with self-test pages and a compact matrix view, the boot banner decodes reset causes and spills a system report, and a release workflow with pre-commit lint locks CI to Node 20. [0215e43, 00af0f1, 399b17, b7c126a, 19fbe9b, 7ee46c7, 702c107]
- **August 25:** SparkFun reference sidebars crash the docs, curating learning links straight from the source. [4e4cf22]
- _Because a README that teaches you nothing is just wall art._

### September — Hardening, Bridge Docs, and Next Spin Prep

- **Early September:** Documented the `bridge/` scripts so serial telemetry nerds can plug in without reverse engineering the handshake.
- Firmware clean-up blitz: reorganized diagnostic docs, annotated the boot banner logic, and dialed in Unity test expectations so the CI lights stay green.
- Hardware planning notes capture the tweaks for a potential `MN42-1.1` spin—beefier regulator headroom and tidier pot footprints are on deck.
- Wrote this "how to read" primer and restructured the month blocks so new contributors can ramp faster than we did.
- _Next milestone: line up the manufacturing quote without losing the DIY ethos._

### October — Docs, Discipline, and Demo Energy

- Started the month by re-reading every major README and HISTORY entry, then tightening cross-links so the repo feels more like a synth-building field guide than a junk drawer.
- Added explicit breadcrumbs from the docs back to the PlatformIO test flow so contributors can jump straight from prose into `pio -d firmware test -e teensy40_unity -vvv` reps without guesswork.
- Captured the debugging rituals we keep repeating—scoped button-matrix traces, WebSerial console macros, and the “use Serial1, not Serial” mantra—so future us stops spelunking commit logs for the same hints.
- Logged this update right as it landed because history that isn’t timestamped when the solder fumes are still in the air might as well be fiction.
- _Documentation isn’t a tombstone; it’s a mosh pit and we just pushed closer to the stage._
- **Mid October:** ConfigManager now pushes its stored MIDI channel map straight into the live managers, PotentiometerManager tracks the same EEPROM offsets, and the globals JSON loader grabbed extra breathing room so oversized hardware presets stop choking the boot sequence.
- **Mid October:** `test_system_report.cpp` pipes its Unity chatter through the same `PrintTarget`/`Serial1` combo the rest of the harness uses, dodging the phantom output problem that kept us guessing.
- **Late October:** Dropped `demo_button_ef_usb_midi.cpp`, a button-plus-envelope USB MIDI sketch that shoves note-ons at your DAW while the envelope follower breathes CC sweeps—perfect for sanity checking the transport without hauling in the full rig.
- **October 31:** Punched `v0.2.0` out the door with the diagnostic boot pages, verbose reset banner, USB MIDI demo, and the fresh release/CI discipline baked into the repo so contributors inherit a rig that actually proves itself.
- _Feels good when the tooling jams as hard as the hardware—documentation, tests, and demo riffs all screaming through the same signal chain._

### November — Teaching Pass & Crash Course Maps

- Pulled a comment pass across the entire firmware stack so every major `.cpp`
  reads like a guided lab—why we clamp enums, how the mux settle time works,
  what "ownership" means for vectors living in global scope.
- Locked in the new slot-ownership ledger and EEPROM layout: slots now declare
  which manager owns them, their SysEx templates ship with each record, and the
  `0x0004` schema reserves dedicated pages so migrations don't trample saved
  presets.
- Root README sprouted a "Firmware Stack Crash Course" pointing straight at the
  annotated sources. The firmware README now unpacks the pointer/ownership
  philosophy for folks learning embedded design by building weird instruments.
- CHANGELOG and HISTORY entries (this one!) flag the documentation shift so
  future builders know exactly when the repo turned into half notebook, half
  teaching guide.
- Follow-up docs drop a Field Guide + tour script so mentors can walk builders
  through the annotated files in a single sitting—think of it as office-hours
  crib notes baked into the repo.
- Lesson headers now call out the subjects each annotated block covers: boot
  choreography, mux scanning, EEPROM hygiene, signal flow math, and MIDI queue
  arbitration all get their own riffs so teachers can stitch together bespoke
  labs.

#### Mid November — Playwright Dress Rehearsal

- Ran the Playwright-driven simulator through a full dress rehearsal so the
  teaching tour has a live lab to point at, not just screenshots. The script
  walks the handshake, forces the schema validation failure branch, replays a
  checksum/ACK rollback, and slaps the simulator toggle to prove each flow in
  the new spec actually screams in the browser.
- Logged the win right in the tooling: `npm --prefix App test` is now wired into
  `test.sh` and the CI workflows, so the same command new contributors run on
  their laptops is the guardrail that keeps the rig honest. Pull it up when you
  teach the module—the prose invites, the command gives them a riff to play.

#### Late November — WebSerial Parity Check

- The WebSerial dashboard now speaks the same manifest dialect as
  `CONFIG_VERSION` `0x0004`: the browser reads `git_sha`, `build_time`,
  `free_ram`, and `free_flash` straight from the handshake so the migration
  dialog stops screaming false mismatches.
- The editor ditched the 42-cell LED colour grid in favour of a brightness +
  hex picker that mirrors the firmware’s single `led` payload, and the local
  schema advertises version `4` to match the firmware’s `GET_MANIFEST` story.
- Runtime normalization backfills follower assignments from slot payloads and
  the new `envelopes` block, keeping the staged diff/apply loop locked in step
  with the slot patches the firmware now emits.

### December — Full-stack Autopilot & Migration Drills

- The `firmware/system_test/mn42_fullstack_runner.js` script graduates from
  prophecy to practice: it now spawns the bridge for you, confirms the HELLO
  handshake, slings a slot patch over OSC, and tails the telemetry stream
  before handing back JSON + text logs (`logs/system-test.*`) you can stash in
  CI artifacts or your lab notebook.
- Docs finally admit the automation exists—`docs/TESTING.md` spells out the
  upload → runner flow so workshop crews don’t have to reverse engineer the
  gauntlet.
- Playwright coverage grows teeth: the headless simulator now rewrites the
  manifest mid-run to trigger the migration dialog, rehearses rollback, and
  proves a clean apply clears the diff badge. `npm --prefix App test` doubles as
  a migration-fire drill you can run without touching hardware.

## 2026

### January — Dual LFO Engines & Routing

- Firmware now instantiates a dedicated `LFOManager` inside `firmware_main.cpp`, seeds two routed oscillators (LED brightness, arp swing, EF gain trim), and mirrors the normalized bus onto shared globals so OLED diagnostics and WebSerial telemetry see the same heartbeat. [2fe3c15]
- _The rig finally has the pulse I imagined—two synced modulators that can flow into LEDs, the arpeggiator, MIDI CCs, or every envelope follower tweak without the scheduler hitching a beat._
- Unity/LFO tests now cover shapes, clock sync ratios, and normalized outputs so the oscillators stay honest even when MIDI clock vanishes, and EEPROM-backed profile payloads save the shape/sync/route snapshots between boots. [2fe3c15]

### January — Web Configurator Build-Out

- The App now splits the runtime kernel from the BenzKnobz view layer, introducing schema-driven forms, a staged diff+rollback panel, telemetry cards, a MIDI monitor, and a simulator toggle rooted in the refreshed `runtime.js`. [2fe3c15]
- _With the simulator standing in for the Teensy and schema validation guarding every Apply, the browser is no longer just a read-only log—it edits profiles, replays slot patches, and rehearses checksum rollbacks before you touch the hardware._
- Playwright specs now vet the config forms, MIDI monitor, bridge handshake, and migration dance so CI can prove the UI without a human in Chromium. [2fe3c15]
- Docs (README, Builder’s Handbook, new `docs/profiles-ui.png`) now explain the runtime contract and UI affordances so field labs can riff off the browser without reverse-engineering the glue code. [2fe3c15]

### January — Serial Command Dispatch Refactor

- Replaced the massive `processCommandQueue` ladder with a sorted dispatch table, dedicated handler functions, and a reentrant `dispatchCommand` so serial commands stay heap-free and easy to extend; the dispatcher now logs unknown commands along with the available list and is exposed to Unity tests via `testOnly_dispatchCommand`. [current change]
- Added `test/test_protocol_dispatch.cpp` plus Unity hooks that hit both a known `HELLO` command and an unknown command to keep the dispatch layer covered even when the rest of the firmware sleeps. [current change]
- Introduced a tiny `firmware/python` shim that points `python` to `python3` so PlatformIO’s `-c` helper and CI-generated test scripts keep running even in environments that only ship `python3`. [current change]

### February — Macro & Scene Snapshots

- Drafted the WebSerial and firmware plumbing so slot 254 can host a macro snapshot: new `SAVE_MACRO_SLOT`/`RECALL_MACRO_SLOT` commands, UI buttons near the profile panel, inline success/failure cues, and optional `GET_CONFIG` refreshes keep the browser in sync without overwriting the active profile.
- Spec'd richer LED feedback with `LedMode` (Static/PeakHold/Trail/ClockPulse), a scheduler-driven `LedAnimator`, diagnostics overrides, and RPC/UI controls so pots and envelopes can pulse, hold, or trail visually without touching interrupt context.
- Laid out the scene recall system: fixed-size `Scene` records that bundle a saved `ConfigState` plus a 16‑byte name, EEPROM-backed slots (4–8), `SAVE_SCENE`/`RECALL_SCENE` verbs, and a planned “Scenes” WebSerial tab so performers can snapshot complete state safely and recall by name.

### February — Onboarding UX, Bridge Usability, and v1.0 Release Prep

- Introduced a persisted Basic/Advanced App UI mode so first-time users can focus on plain knob-to-MIDI mapping while experienced users still get full EF/ARG/filter tooling.
- Added glossary/help cues directly in slot editors and schema-rendered forms so terms like EF, ARG, and SysEx placeholders are explained where users actually click.
- Profile workflow got a guided lane (switch target slot -> apply staged edits -> save slot), plus safer RPC gating and clearer connect/disconnect behavior across profile, macro, and scene controls.
- Bridge docs were rebuilt for real-world onboarding: quickstart, performer one-sheet, full operator reference, packaging roadmap, and release artifact checklist now cross-link from docs index and release guide.
- Added bridge release-prep scripts (`smoke_bridge_cli.js`, `package_bridge.sh`) and npm aliases so packaging work can start from repeatable commands instead of ad-hoc shell history.
- Hardware CAD iteration files are now ignored by default (`.epro/.eprj/autosave/_backup`) to keep repository release surfaces focused on firmware/app/docs while local board iteration remains private.
- Ran a deep explainability pass over the post-2025 runtime/protocol/scheduler/config code paths so future contributors can read intent (ordering, recovery, throttling, staging semantics) directly in-source.
- **Commit summary (current v1.0 prep sweep):** newcomer-friendly App UX, profile-flow hardening, bridge onboarding + packaging docs, release tooling/checklists, and maintainability comments across recent architecture work.

### February — Demo Polish Pass (Identity, Readability, Safety)

- WebSerial identity contract now includes a dedicated `device_name` field in `GET_MANIFEST`, letting host apps show explicit device identity instead of relying on guesswork from firmware version strings alone.
- The App connect card now carries a clear `Connected to: <device> (FW <version>)` banner, plus an inline "What to do if connect fails" helper that points users to close competing serial clients, replug USB, and refresh.
- Added focused Playwright coverage (`App/tests/connection_banner.spec.js`) so the identity banner + connect UX path are guarded in CI using the simulator harness.
- OLED labeling got a readability cleanup for demo scenarios: arp shape names expanded to human-readable terms, filter labels de-abbreviated, and swing preset feedback now reports as `Swing: n%`.
- Added an explicit panic-safe exit combo (`Ctrl0 + Ctrl1 + Ctrl2`) that stops arp, disables EF follow, and reloads the active profile baseline so demos can recover quickly from bad live state.
- Added demo-focused App presets (`DEMO_A - Reactive Stack`, `DEMO_B - Clock Contrast`) plus a dedicated runbook (`docs/DemoPolish.md`) covering soak, EXT-clock stress checks, panic-path validation, and asset prep.
- _This pass shifted the demo story from "we can probably recover" to "we can prove identity, readability, and recovery on command because we are being militant about this for a reason."_
- PCBWay reached out and wants to help me make a run of prototypes of the board - motivation to move from simply a cool maker project to something a bit more ornate/rugged.

### March — Bridge Packaging Rollout Plan (Planned)

- Locked the first distribution wave to desktop users on macOS (`arm64` + `x64`), Windows (`x64`), and Linux (`x64`) so we can prioritize predictable installs over broad-but-fragile matrix sprawl.
- Formalized a packaging bake-off between `pkg` and `nexe`: `pkg` is the likely default path for speed + ecosystem fit, while `nexe` stays as the fallback if we need deeper control over native module embedding.
- Called out the remaining bundling blockers explicitly: `serialport` native artifacts, static asset inclusion, runtime config path handling, and license payload colocation inside shipped binaries/installers.
- Defined installer workflow targets per platform so non-technical users do not need Node or npm: signed `.pkg`/`.dmg` for macOS, MSI installer for Windows, and `.deb` + AppImage options for Linux.
- Set the update architecture to channel-based auto-updates (stable/beta) with signed manifests, in-app version checks, and one-click rollback to last-known-good when post-update health checks fail.
- Sequenced the work into four release gates: `G1` reproducible cross-platform builds, `G2` installer QA on clean machines, `G3` staged auto-update dogfood, `G4` public release with operator docs and recovery playbook.
- _Goal: make "download -> install -> connect" feel boringly reliable for performers who should never have to think about JavaScript tooling just to use the bridge._

### March — Release Hardening, Coverage Sweep, and Prototype Fab Handoff

- Release prep shifted from “probably shippable” to “prove it”: deterministic export checks, contract-sync guards, and release artifact validation now pin the App/firmware handshake down so export bundles stop drifting silently. [`c4f3132`, tag `26_1`]
- A noisy round of stale security findings forced a real cleanup pass through the firmware formatting paths: remaining `sprintf` calls were replaced with bounded `snprintf`, Unity link issues were straightened out, and the repo now has fewer fake fires to triage during release review. [`77978db` and follow-up March fixes]
- The configurator stopped advertising unfinished work: the placeholder Elektron Analog Rytm preset entry was pulled from the visible presets list instead of shipping a “TODO” badge in the UI.
- Unity coverage expanded beyond the core math/slot logic into the orchestration layer: `exponentialMovingAverage`, `LedAnimator::cycleMode`, command queue parsing/overflow handling, SeedBox handshake flow, runtime pending note-offs, WebSerial snapshots/slot patches, and the UI tuning helpers now have direct test coverage.
- _The useful surprise here was not just “more tests passed”; it was that broader coverage exposed a real scheduler bug hiding in plain sight._
- `Scheduler.cpp` had several tasks registered as one-shot work instead of recurring tasks; that behavior is now fixed so MIDI service, envelopes, WebSerial streaming, SeedBox updates, and low-priority UI/LED refreshes actually persist after the first scheduled run.
- Added a small unit-test logging sink plus targeted seams around runtime state, control-pot values, and task metadata so orchestration code can be asserted clinically without dragging the whole hardware stack into the Unity harness.
- Hardware momentum crossed a line from CAD iteration to physical commitment: **March 13, 2026:** the prototype PCB run was sent to fabrication, marking the handoff from design churn to waiting on real boards.
- _There is a different kind of seriousness once copper has been ordered; every doc line and every test starts reading like an instruction to your future self standing at a bench with actual hardware in the mail._

### March — Contract Closure, Recovery Truth, and Orientation Lock

- A fit/finish audit forced the repo to stop treating firmware, bridge, App, and docs like separate projects. The useful work was not “make the UI prettier”; it was closing the seams where one layer was still bluffing about another. [`9143195`, `b6844fc`]
- Browser-driven profile save/load/reset is now real firmware behavior rather than a simulator flourish, and the same pass landed actual EEPROM-backed macro snapshots plus scene storage so the recovery story finally matches the operator docs. [`9143195`]
- The bridge/App/firmware command story was tightened until it read like one contract: native transport verbs are the source of truth, bridge live control now uses `SET_SLOT_VALUE`, and the docs stopped narrating old command shapes as if they still shipped. [`9143195`, `b6844fc`]
- Browser-only slot notes got their own line in the sand. `label`, the MIDI badge, and `Take Control` are now explicitly local browser affordances instead of fake device config, and the reconnect path no longer stomps those notes with whatever the last `get_config` happened to say. [`3d3e595`]
- The documentation pass turned into an orientation lock for pre-production: host/browser/DAW compatibility claims now show up early and conservatively, `HostCompatibility.md` was added as the plain-English matrix, and MkDocs strict mode is back to green with nav + reference pages made explicit. [`403874e`, `ff60649`, `8e1deb7`]
- Storage verification also got more honest. Firmware tests grew targeted persistence coverage and the full-system runner learned a destructive `--exercise-storage` path so bench time can prove profile/macro/scene behavior instead of taking the docs on faith. [`b6844fc`]
- _This was the month the repo got less romantic and more trustworthy: fewer implied capabilities, fewer simulator ghosts, more “does the whole instrument tell the same story when you actually use it?”_

## Overview

Across roughly 18 months of commits, MOARkNOBS‑42 has evolved from a set of untested firmware files into a documented DIY MIDI controller complete with hardware PCB schematics, test suites, and detailed usage notes. The repository now houses:

- `firmware/` – Teensy‑based C++ code with modular managers and tests.
- `hardware/` – Design files for the `MN42` board.
- `App/` - Playwright-facilitated configuration app
- Comprehensive README files describing features, wiring, building instructions, and a bit of mania.
