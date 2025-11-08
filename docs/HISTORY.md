# Project History

This file summarizes the development of the MOARkNOBS-42 project based on commit history with a few inserts re: design choices

## How to Read this History

- **Start with the focus tags.** Each block pairs a time range with the main obsession so you know whether you're about to read about PCB copper or MIDI voodoo.
- **Skim the italic callouts** for the "why" behind the commits—they're the diary fragments that explain decisions better than dry hashes ever could.
- **Use the bracketed references** to chase the actual commits or tags if you want to see the gritty diffs.
- **Bridge notes** flag the `bridge/` tooling so you can wire telemetry without cobbling your own serial hacks.

## 2024
- *I decided I wanted to build more instruments and wanted to explore MIDI and microcontrollers more expansively. Heavy early research around simple machines, 1-function mock ups, and C++ best-practices began. Initial functional inspiration: Bastl Instruments '60 Knobs' ([repo](https://github.com/bastl-instruments/60knobs)) and their wild [experimental playground](https://github.com/bastl-instruments/60knobs-experimental)*
- **December 10:** Initial commit introduces the firmware source tree with modules for button scanning, MIDI handling, display control, and EEPROM support. [58ef040]
- **Mid December:** Early work on display management and multiplexed button matrix ("mux management" and "display/managers").
- *More complicated in areas that I didn't anticipate complexity in than in the outright complicated parts*
- **Late December:** Adds EEPROM and USB MIDI functionality, debounce logic, and second revision of the display code. [dbf3c21, 788a1ba]

## 2025

- *How is this thing supposed to happen, as more than just a simple experiment?*
### January – March — Hardware Layout Bootstrapping

- Hardware design begins in KiCad. Multiple board iterations are committed along with early firmware clean‑ups and configuration handling. [70665ff, 4c22a8b]
- *My old workstation only runs up to kiCAD 6*
- By late January the "easyeda" project is created and the firmware tree moves under `firmware/` with substantial PCB files added. [20e518a]
- February and March see continual board revisions (`brd`, `brd v2`), button routing tweaks, and resistor pull-up improvements. [f36d220, dd0ebb6]

### April — Repo Reorg & First Board Spin

- Repository reorganization and creation of the first official PCB revision `MN42-1` in EasyEDA. [174b516, f5916a7]
- *Work just a little at a time, and eventually you have a whole castle*

### May — Firmware Refine & Test Harness

- Firmware refactors focus on display timing, envelope follower behavior, and general stability. README updates document usage. [44e89dc, ec7edb3]
- Initial testing framework appears along with incremental fixes. [ed357da, 6a4e54a]
- *Getting onto breadboards for timing testing/display/midi/etc.*

### June — Filters, MIDI Depth, & Documentation

- Early June introduces unit tests for the BiquadFilter and bug fixes around fade animations. [b050f4f, 69fd0db]
- **June 17:** Tagged as `firmware1.0`, marking a stable baseline before adding more features. [04f6b72]
- *I bet this can do so much more than CC really easy*
- Following this, MIDI capabilities expand significantly with new message types and slot verification. [6cef527, 0e5c9d9]
- **June 23:** MIDI type combos and filter tuning feedback are implemented, and documentation is expanded. [2615616, ab8078a]
- *Decided to give AI tools a try because who am I to say no to a perfectly good tool?*
- Extensive cleanup on June 25 consolidates pin assignments, removes old files, adds the MIT license, and updates hardware docs. [bfbfbaf, cb39705]
- Additional DSP tests and control button wiring verification land on June 26. [b703f1f, 1a2b239]
- Late June brings documentation improvements describing envelope filters, ARG mode, and exhaustive testing approaches. [43b1d33, b14ef44]
- **June 28:** Final `readme tweaks` commit wraps up the documented state of the project. [8ed5568]
- *^^^ see what I mean? I am tyring to get better at documentation as I go, but this helps explain what is happening so much more than my drafts alone. I just have an assistant now.*

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
- *If you ever wanted a synth to show you its source and its soul, this is the moment.*
- Wired up the filter‑tuning pot and roughed up the arpeggiator—hands‑on analog control meets sequencer swagger, plus a reminder that drift and off‑by‑ones are always lurking.
- **August 6:** First public drop lands as `v0.1.0`, bundling filter‑tuning pots, a self‑driving arpeggiator, WebSerial telemetry, NRPN/RPN/SysEx support, and the project's inaugural `CHANGELOG`. [v0.1.0]
- **August 8:** Merged PR #286, hauling in the full FastLED arsenal and scribbling teachable comments all over `firmware/App/benzknobz.html`. [956069c]
- **August 11** My ears are still ringing because I went to a metal show in the basement of an American Legion. I am scrambling to make these Unity testers work for the codebase as well as actual builds of the firmware.
- *Standards are not optional; often the best lessons come from coloring off the page.*
- **Late August:** CI kept flaking out, so the build scripts took a beating. Swapped the MIDI library, chased phantom `usb_midi` ghosts, and bolted on a Unity test rig so every commit has to prove itself.
- **August 22–23:** Diagnostic mode lands with self-test pages and a compact matrix view, the boot banner decodes reset causes and spills a system report, and a release workflow with pre-commit lint locks CI to Node 20. [0215e43, 00af0f1, 399b17, b7c126a, 19fbe9b, 7ee46c7, 702c107]
- **August 25:** SparkFun reference sidebars crash the docs, curating learning links straight from the source. [4e4cf22]
- *Because a README that teaches you nothing is just wall art.*

### September — Hardening, Bridge Docs, and Next Spin Prep

- **Early September:** Documented the `bridge/` scripts so serial telemetry nerds can plug in without reverse engineering the handshake.
- Firmware clean-up blitz: reorganized diagnostic docs, annotated the boot banner logic, and dialed in Unity test expectations so the CI lights stay green.
- Hardware planning notes capture the tweaks for a potential `MN42-1.1` spin—beefier regulator headroom and tidier pot footprints are on deck.
- Wrote this "how to read" primer and restructured the month blocks so new contributors can ramp faster than we did.
- *Next milestone: line up the manufacturing quote without losing the DIY ethos.*

### October — Docs, Discipline, and Demo Energy

- Started the month by re-reading every major README and HISTORY entry, then tightening cross-links so the repo feels more like a synth-building field guide than a junk drawer.
- Added explicit breadcrumbs from the docs back to the PlatformIO test flow so contributors can jump straight from prose into `pio -d firmware test -e teensy40_unity -vvv` reps without guesswork.
- Captured the debugging rituals we keep repeating—scoped button-matrix traces, WebSerial console macros, and the “use Serial1, not Serial” mantra—so future us stops spelunking commit logs for the same hints.
- Logged this update right as it landed because history that isn’t timestamped when the solder fumes are still in the air might as well be fiction.
- *Documentation isn’t a tombstone; it’s a mosh pit and we just pushed closer to the stage.*
- **Mid October:** ConfigManager now pushes its stored MIDI channel map straight into the live managers, PotentiometerManager tracks the same EEPROM offsets, and the globals JSON loader grabbed extra breathing room so oversized hardware presets stop choking the boot sequence.
- **Mid October:** `test_system_report.cpp` pipes its Unity chatter through the same `PrintTarget`/`Serial1` combo the rest of the harness uses, dodging the phantom output problem that kept us guessing.
- **Late October:** Dropped `demo_button_ef_usb_midi.cpp`, a button-plus-envelope USB MIDI sketch that shoves note-ons at your DAW while the envelope follower breathes CC sweeps—perfect for sanity checking the transport without hauling in the full rig.
- **October 31:** Punched `v0.2.0` out the door with the diagnostic boot pages, verbose reset banner, USB MIDI demo, and the fresh release/CI discipline baked into the repo so contributors inherit a rig that actually proves itself.
- *Feels good when the tooling jams as hard as the hardware—documentation, tests, and demo riffs all screaming through the same signal chain.*


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

## Overview

Across roughly nine months of commits, MOARkNOBS‑42 evolved from a set of untested firmware files into a documented DIY MIDI controller complete with hardware PCB designs, test suites, and detailed usage notes. The repository now houses:

- `firmware/` – Teensy‑based C++ code with modular managers and tests.
- `hardware/` – EasyEDA design files for the `BTN_42` button matrix.
- Comprehensive README files describing features, wiring, and building instructions.

