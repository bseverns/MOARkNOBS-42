# Project History

> **Doc class:** Historical narrative. This page is not the source of current contract, support, release, or test truth. When this page disagrees with current README, reference, validation, or release pages, the current contract and evidence pages win. See [Documentation Truth Map](../reference/DocumentationTruthMap.md).

This file tells the development story of MOARkNOBS-42: why the repo sprawled, when the hardware got serious, and how the project turned from a pile of firmware experiments into a documented hardware-test package. It is based on commit history with a few first-person design notes left intact on purpose.

## Short Version

MOARkNOBS-42 started in late 2024 as an attempt to build more instruments, learn MIDI deeply, and push microcontroller work past one-function sketches. Through 2025 it became a real Teensy-based MIDI controller project with a PCB, display, EEPROM-backed configuration, envelope followers, arpeggiator behavior, WebSerial telemetry, and an expanding test harness.

By early 2026, the center of gravity moved from "can the firmware do this?" to "can the whole instrument prove this?" Firmware, App, Bridge, docs, release tooling, and bench evidence started being treated as one product surface. The current repo posture is a hardware-test package, not a beta, public, or fabrication-ready release.

The useful arc is:

- **2024 to early 2025:** exploration, firmware scaffolding, KiCad/EasyEDA hardware starts.
- **Spring and summer 2025:** first board spins, firmware baseline, MIDI expansion, WebSerial, and early tagged drops.
- **Fall 2025:** CI, diagnostics, docs, teaching structure, and bridge/test discipline.
- **Winter 2025 to early 2026:** browser configurator, LFOs, macro/scene storage, and safer live recovery.
- **March to May 2026:** release hardening, prototype fabrication handoff, contract cleanup, Node 24 bridge path, and evidence-driven release gates.

## How to Read This History

- **Read phases, not every hash.** Each phase names the main obsession so you know whether you are in copper, firmware, browser, Bridge, or release-proof territory.
- **Treat "Turning point" callouts as the story spine.** They mark moments where the project changed shape, not just moments where code landed.
- **Use commit references as trailheads.** Short hashes and tags are included where they already existed in the old history; missing hashes are labeled with durable topic names instead of volatile placeholders.
- **Read reflections as diary fragments.** The italic sections keep the first-person design voice, grouped away from the factual bullets so the page scans without losing its pulse.
- **Use contract docs for current truth.** This page remembers the path. It does not define today's protocol, support boundary, release status, or test requirements.

## Completed History

### Phase 1: Origin, Firmware Skeleton, and Board Curiosity (Late 2024 - March 2025)

The project begins as a hands-on MIDI and microcontroller study, with Bastl Instruments' [60 Knobs](https://github.com/bastl-instruments/60knobs) and [60knobs-experimental](https://github.com/bastl-instruments/60knobs-experimental) acting as early sparks.

- **December 10, 2024:** The initial commit introduces the firmware source tree with modules for button scanning, MIDI handling, display control, and EEPROM support. [58ef040]
- **Mid December 2024:** Early display management, multiplexed button matrix work, "mux management," and display managers start forming the first runtime shape.
- **Late December 2024:** EEPROM, USB MIDI functionality, debounce logic, and a second display-code revision land. [dbf3c21, 788a1ba]
- **January - March 2025:** Hardware design starts in KiCad, then shifts into repeated board iteration. Firmware cleanup and configuration handling continue alongside the first serious PCB files. [70665ff, 4c22a8b]
- **Late January 2025:** The EasyEDA project appears, the firmware tree moves under `firmware/`, and substantial board files arrive. [20e518a]
- **February - March 2025:** Board revisions, button routing, and resistor pull-up improvements keep cycling. [f36d220, dd0ebb6]

> **Turning point:** The project stops being only a firmware experiment once the repo has to care about physical layout, not just whether a sketch compiles.

**Reflections from this stretch**

- _I decided I wanted to build more instruments and wanted to explore MIDI and microcontrollers more expansively. Heavy early research around simple machines, one-function mockups, and C++ best practices began._
- _More complicated in areas that I didn't anticipate complexity in than in the outright complicated parts._
- _How is this thing supposed to happen, as more than just a simple experiment?_
- _My old workstation only ran up to KiCad 6._

### Phase 2: First Board Spin and Firmware Baseline (April - June 2025)

Spring turns the repo into something with named hardware, testable firmware pieces, and enough documentation to begin carrying intent forward instead of relying on memory.

- **April 2025:** Repository reorganization and the first official PCB revision, `MN42-1`, land in EasyEDA. [174b516, f5916a7]
- **May 2025:** Firmware refactors focus on display timing, envelope follower behavior, and general stability. README updates start documenting actual use. [44e89dc, ec7edb3]
- **May 2025:** The initial testing framework appears with incremental fixes. [ed357da, 6a4e54a]
- **Early June 2025:** Unit tests for `BiquadFilter` arrive, along with fade-animation fixes. [b050f4f, 69fd0db]
- **June 17, 2025:** Tag `firmware1.0` marks a stable firmware baseline before the feature set widens. [04f6b72]
- **Late June 2025:** MIDI message support expands beyond plain CCs, slot verification appears, filter feedback improves, and docs explain envelope filters, ARG mode, and test approaches. [6cef527, 0e5c9d9, 2615616, ab8078a, 43b1d33, b14ef44]
- **June 25 - 28, 2025:** Pin assignments are consolidated, old files are removed, the MIT license lands, hardware docs improve, DSP/control-button tests expand, and README cleanup wraps the documented state. [bfbfbaf, cb39705, b703f1f, 1a2b239, 8ed5568]

> **Turning point:** `firmware1.0` is less "done" than "stable enough to mutate." After this, the project has a baseline to branch from instead of a fog bank.

**Reflections from this stretch**

- _Work just a little at a time, and eventually you have a whole castle._
- _Getting onto breadboards for timing testing/display/MIDI/etc._
- _I bet this can do so much more than CC really easy._
- _Decided to give AI tools a try because who am I to say no to a perfectly good tool?_
- _I am trying to get better at documentation as I go, but this helps explain what is happening so much more than my drafts alone. I just have an assistant now._

### Phase 3: Feature Expansion, Public Drops, and Test Discipline (July - August 2025)

Summer is where MOARkNOBS-42 starts acting like an instrument instead of a hardware exercise. The arpeggiator, deeper MIDI types, WebSerial telemetry, diagnostics, and CI/test expectations all arrive in a noisy cluster.

- **July 2025:** Arpeggiator mode lands with base-note support and full MIDI type coverage. PlatformIO builds pick up the arpeggiator sources, README behavior gets documented, and tests expand across biquad filter modes and EEPROM slot verification. [850b4e9, 60abd55, e03466d, 6b81ed0, 1e9a015, 09ffda2, 2716465, 9a8733b]
- **July 2025:** Button-driven navigation simplifies test phases, helper functions unify test initialization, variables get clearer, and docs add flashing instructions, diagrams, and links back to this history. [8716674, bf1936f, 109a1b8, 6d769ec, 822a1ea, 5f3e34b, e25622e, 0941fcb]
- **Early August 2025:** Hardware v1.02 work completes, `Utility::processBulkUpdate` is refactored around a raw char buffer, and `MN42_v2` design assets arrive under hardware references. `fea338e` calls for thicker LED power traces so the LEDs do not sag or cook when current rises.
- **Early August 2025:** NRPN, RPN, and raw SysEx support widen the MIDI story; WebSerial starts streaming runtime state to the browser.
- **August 6, 2025:** Early tagged drop `v0.1.0` bundles filter-tuning pots, a self-driving arpeggiator, WebSerial telemetry, NRPN/RPN/SysEx support, and the first `CHANGELOG`. [v0.1.0]
- **August 8, 2025:** PR #286 brings in FastLED and adds teachable comments around the early browser configurator path. [956069c]
- **Late August 2025:** CI flakes force build-script hardening. The MIDI library changes, `usb_midi` ghosts get chased down, and a Unity test rig makes commits prove themselves.
- **August 22 - 23, 2025:** Diagnostic mode lands with self-test pages, compact matrix view, reset-cause boot banner, system report, release workflow, pre-commit lint, and CI pinned to Node 20 at the time. [0215e43, 00af0f1, 399b17, b7c126a, 19fbe9b, 7ee46c7, 702c107]
- **August 25, 2025:** SparkFun reference sidebars join the docs as learning links from the source. [4e4cf22]

> **Turning point:** `v0.1.0` is the first outward signal that the project is not just private bench noise. The repo now has tags, browser telemetry, deeper MIDI, and a test culture that can hurt your feelings usefully.

**Reflections from this stretch**

- _If you ever wanted a synth to show you its source and its soul, this is the moment._
- _My ears are still ringing because I went to a metal show in the basement of an American Legion. I am scrambling to make these Unity testers work for the codebase as well as actual builds of the firmware._
- _Standards are not optional; often the best lessons come from coloring off the page._
- _Because a README that teaches you nothing is just wall art._

### Phase 4: Diagnostics, Bridge Notes, and Teaching Repo (September - December 2025)

Fall turns the repo into a field guide. The work is still technical, but the bigger shift is that bring-up rituals, bridge behavior, annotated source, and repeatable tests become first-class material.

- **September 2025:** `bridge/` scripts get documented so serial telemetry can be wired without reverse-engineering the handshake. Diagnostic docs, boot banner logic, and Unity expectations are cleaned up.
- **September 2025:** Hardware planning notes capture possible `MN42-1.1` changes: regulator headroom, pot footprints, and related spin prep.
- **October 2025:** Major README and HISTORY passes tighten cross-links, PlatformIO test breadcrumbs, scoped button-matrix rituals, WebSerial console macros, and the "use Serial1, not Serial" Unity transport lesson.
- **Mid October 2025:** `ConfigManager` pushes stored MIDI channel maps into live managers, `PotentiometerManager` tracks the same EEPROM offsets, the globals JSON loader gets more room for hardware presets, and `test_system_report.cpp` routes Unity chatter through the same `PrintTarget`/`Serial1` combo as the rest of the harness.
- **Late October 2025:** `demo_button_ef_usb_midi.cpp` provides a button-plus-envelope USB MIDI sanity sketch.
- **October 31, 2025:** `v0.2.0` ships diagnostic boot pages, verbose reset banner, USB MIDI demo, and release/CI discipline.
- **November 2025:** Firmware comments get a teaching pass across major `.cpp` files. Slot ownership, EEPROM layout, SysEx templates, schema page reservations, source tour material, and lesson headers turn the repo into half notebook, half lab.
- **Mid November 2025:** The Playwright-driven simulator rehearses handshake, schema validation failure, checksum/ACK rollback, and simulator toggles. `npm --prefix App test` gets wired into `test.sh` and CI.
- **Late November 2025:** WebSerial aligns with `CONFIG_VERSION` `0x0004`; the browser reads `git_sha`, `build_time`, `free_ram`, and `free_flash` from the handshake. The LED editor moves from a 42-cell color grid to brightness plus hex color, and runtime normalization keeps staged diff/apply behavior aligned with firmware slot patches.
- **December 2025:** `firmware/system_test/mn42_fullstack_runner.js` starts spawning the Bridge, confirming `HELLO`, sending slot patches over OSC, tailing telemetry, and writing JSON/text logs. Docs describe the upload-to-runner flow, and Playwright migration coverage grows teeth.

> **Turning point:** The repo stops treating docs as a recap and starts treating them as part of the instrument. A future builder can learn from source comments, tours, tests, and runner evidence instead of spelunking old commits.

**Reflections from this stretch**

- _Next milestone: line up the manufacturing quote without losing the DIY ethos._
- _Documentation isn't a tombstone; it's a mosh pit and we just pushed closer to the stage._
- _Feels good when the tooling jams as hard as the hardware: documentation, tests, and demo riffs all screaming through the same signal chain._

### Phase 5: Configurable Instrument and Live-Recovery Shape (January - February 2026)

Early 2026 is where the system starts behaving like a configurable performance object. LFO routing, browser-side editing, serial dispatch cleanup, macro/scene storage, onboarding UX, and demo safety all move forward together.

- **January 2026:** Firmware instantiates a dedicated `LFOManager`, seeds two routed oscillators for LED brightness, arp swing, EF gain trim, and shared diagnostics/telemetry globals. Unity/LFO tests cover shapes, clock sync ratios, normalized outputs, MIDI-clock loss, and EEPROM-backed route snapshots. [2fe3c15]
- **January 2026:** The App splits the runtime kernel from the BenzKnobz view layer, adding schema-driven forms, staged diff/rollback UI, telemetry cards, MIDI monitor, simulator toggle, and Playwright coverage for config forms, bridge handshake, MIDI monitor, and migration flow. [2fe3c15]
- **January 2026:** Serial command handling is refactored from a massive `processCommandQueue` ladder into a sorted dispatch table with dedicated handlers, `dispatchCommand`, unknown-command logging, and Unity coverage through `testOnly_dispatchCommand`. [dispatch-refactor-2026-01]
- **January 2026:** `test/test_protocol_dispatch.cpp` covers known `HELLO` dispatch and unknown commands. A small `firmware/python` shim points `python` to `python3` for PlatformIO helper compatibility in `python3`-only environments. [dispatch-refactor-2026-01]
- **February 2026:** Macro and scene snapshot plumbing starts: `SAVE_MACRO_SLOT`, `RECALL_MACRO_SLOT`, richer LED mode planning, scheduler-driven LED animation, fixed-size scene records, EEPROM-backed slots, and browser controls begin taking shape.
- **February 2026:** The App gains persisted Basic/Advanced UI modes, glossary/help cues, safer profile workflow, RPC gating, and clearer connect/disconnect behavior across profile, macro, and scene controls.
- **February 2026:** Bridge onboarding docs get rebuilt with quickstart, performer one-sheet, operator reference, packaging roadmap, release artifact checklist, smoke scripts, package scripts, and npm aliases.
- **February 2026:** CAD iteration files such as `.epro`, `.eprj`, autosave, and backup outputs are ignored by default so release surfaces stay focused.
- **February 2026:** Demo polish adds a `device_name` manifest field, clear "Connected to" banner, connect-failure help, Playwright coverage for connection UX, more readable OLED labels, a panic-safe exit combo (`Ctrl0 + Ctrl1 + Ctrl2`), demo presets, and a runbook in `docs/validation/DemoPolish.md`.

> **Turning point:** The browser is no longer only a log window. It becomes a guarded editor that can rehearse changes, validate schemas, recover from bad live state, and expose the instrument to less-technical users without lying about what is happening underneath.

**Reflections from this stretch**

- _The rig finally has the pulse I imagined: two synced modulators that can flow into LEDs, the arpeggiator, MIDI CCs, or every envelope follower tweak without the scheduler hitching a beat._
- _With the simulator standing in for the Teensy and schema validation guarding every Apply, the browser is no longer just a read-only log; it edits profiles, replays slot patches, and rehearses checksum rollbacks before you touch the hardware._
- _This pass shifted the demo story from "we can probably recover" to "we can prove identity, readability, and recovery on command because we are being militant about this for a reason."_
- _PCBWay reached out and wants to help me make a run of prototypes of the board: motivation to move from simply a cool maker project to something a bit more ornate and rugged._

### Phase 6: Release Hardening, Contract Closure, and Prototype Handoff (March - May 2026)

Spring 2026 is the "prove it" season. The project tightens export provenance, test coverage, Bridge, App, and firmware contracts, storage truth, host compatibility, and hardware evidence.

- **March 2026:** Release prep shifts from "probably shippable" to "prove it" through deterministic export checks, contract-sync guards, and release artifact validation. [`c4f3132`, tag `26_1`]
- **March 2026:** Stale security findings force cleanup in firmware formatting paths: remaining `sprintf` calls are replaced with bounded `snprintf`, Unity link issues are straightened out, and false-positive triage noise drops. [`77978db` and March follow-ups]
- **March 2026:** The configurator stops advertising unfinished placeholder presets, including the visible Elektron Analog Rytm stub.
- **March 2026:** Unity coverage expands into orchestration: `exponentialMovingAverage`, `LedAnimator::cycleMode`, command queue parsing and overflow, SeedBox handshake flow, runtime pending note-offs, WebSerial snapshots/slot patches, and UI tuning helpers.
- **March 2026:** A scheduler bug is exposed and fixed: MIDI service, envelopes, WebSerial streaming, SeedBox updates, and low-priority UI/LED refreshes are registered as recurring work instead of one-shot tasks.
- **March 13, 2026:** The prototype PCB run is sent to fabrication, marking the handoff from design churn to real boards in the mail.
- **March 2026:** A fit/finish audit forces firmware, Bridge, App, and docs to tell one story. Browser-driven profile save/load/reset becomes real firmware behavior, EEPROM-backed macro snapshots and scene storage land, native transport verbs become the source of truth, and Bridge live control moves to `SET_SLOT_VALUE`. [`9143195`, `b6844fc`]
- **March 2026:** Browser-only slot notes such as `label`, the MIDI badge, and `Take Control` are explicitly local browser affordances instead of fake device config. Reconnect behavior stops stomping local notes with the last `GET_CONFIG` response. [`3d3e595`]
- **March 2026:** `HostCompatibility.md` is added, MkDocs strict mode returns to green, and navigation/reference pages become explicit. [`403874e`, `ff60649`, `8e1deb7`]
- **March 2026:** Firmware tests gain targeted persistence coverage, and the full-system runner gets a destructive `--exercise-storage` path for profile/macro/scene proof. [`b6844fc`]
- **April - May 2026:** Source exports come from tracked files only; release manifests ignore generated dirt while still refusing dirty tracked trees. [release-gate-tightening-2026-04]
- **April - May 2026:** Doctor and App bridge paths align around Node 24, matching the Bridge release lane and current host evidence. [node24-bridge-alignment-2026-04]
- **April - May 2026:** Release verification treats Bridge and App tests as mandatory, layers in Unity and full-stack HIL when hardware is present, and records skipped-versus-run evidence in `release_verification.json`. [release-verification-evidence-2026-04]
- **April - May 2026:** Bridge packaging adds gated signing/notarization hooks so beta/public artifacts must prove signing before being treated as outward-facing releases. [bridge-signing-gates-2026-04]

> **Turning point:** Once copper is ordered and release artifacts are gated, the project has to stop narrating intentions as if they are proof. The repo starts saying what is verified, what is planned, and what is merely historical.

**Reflections from this stretch**

- _The useful surprise here was not just "more tests passed"; it was that broader coverage exposed a real scheduler bug hiding in plain sight._
- _There is a different kind of seriousness once copper has been ordered; every doc line and every test starts reading like an instruction to your future self standing at a bench with actual hardware in the mail._
- _This was the month the repo got less romantic and more trustworthy: fewer implied capabilities, fewer simulator ghosts, more "does the whole instrument tell the same story when you actually use it?"_
- _The release path stopped being "build some zips" and became "prove the source, prove the host, and prove the outward-facing binaries are treated like evidence artifacts instead of internal scraps."_

## Current Repository Shape

This is an orientation snapshot, not a contract. For current status, use [Repository Contents](RepositoryContents.md), [Release Boundary Index](../release/ReleaseBoundaryIndex.md), and [TESTING](../validation/TESTING.md).

- `firmware/`: Teensy 4.0 PlatformIO project with modular firmware managers, Unity tests, and hardware/system test runners.
- `hardware/`: current hardware references, substitutions, parts rationale, and current-build notes. Historical PCB source exports are not the same thing as a public fabrication-ready hardware package.
- `App/`: browser configurator with Playwright coverage, runtime kernel, BenzKnobz view layer, simulator support, and WebSerial plus Bridge paths.
- `bridge/`: unsigned desktop Bridge CLI/server for serial, OSC, MIDI, and browser-served workflows.
- `docs/`: MkDocs source for learning, use, build, proof, reference, project, and archive material.
- `tools/`: guardrails for contract sync, release readiness, documentation checks, and repo health.

## Planned Roadmap

This section is deliberately separate from completed history. It records direction, not shipped truth.

### Bridge Packaging Rollout

- Keep the first distribution wave focused on desktop users: macOS (`arm64` and `x64`), Windows (`x64`), and Linux (`x64`).
- Continue the `pkg` versus `nexe` bake-off. `pkg` remains the likely default path for speed and ecosystem fit; `nexe` stays as fallback if native module embedding needs deeper control.
- Resolve bundling blockers around `serialport` native artifacts, static asset inclusion, runtime config paths, and license payload colocation.
- Target installer workflows that do not require non-technical users to install Node or npm: signed `.pkg`/`.dmg` for macOS, MSI for Windows, and `.deb` plus AppImage options for Linux.
- Build toward channel-based signed updates with stable/beta lanes, in-app version checks, and rollback to last-known-good after failed health checks.
- Keep the release gates explicit: `G1` reproducible cross-platform builds, `G2` installer QA on clean machines, `G3` staged auto-update dogfood, and `G4` public release with operator docs and recovery playbook.

### Hardware-Test to Beta Evidence

- Continue turning bench receipts into dated evidence instead of broad claims.
- Keep hardware-in-the-loop checks honest about what board, power profile, host OS, browser, and Bridge path were actually used.
- Avoid widening public support language until release criteria and validation docs have the proof to back it.

### Operator Experience

- Keep reducing the gap between performer language and protocol language without hiding the protocol.
- Continue polishing profile, macro, scene, and panic-recovery flows around real hardware behavior.
- Make "download, install, connect" boringly reliable for performers who should never have to think about JavaScript tooling just to use the Bridge.

**Roadmap reflection**

- _Goal: make the setup feel boringly reliable without sanding off the handmade instrument underneath._
