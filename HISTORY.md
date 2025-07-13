# Project History

This file summarizes the development of the MOARkNOBS-42 project based on commit history with a few inserts re: design choices

## 2024
- *I decided I wanted to build more instruments*
- **December 10:** Initial commit introduces the firmware source tree with modules for button scanning, MIDI handling, display control, and EEPROM support. [58ef040]
- **Mid December:** Early work on display management and multiplexed button matrix ("mux management" and "display/managers").
- *More complicated in areas that I didn't anticipate complexity in than in the outright complicated parts*
- **Late December:** Adds EEPROM and USB MIDI functionality, debounce logic, and second revision of the display code. [dbf3c21, 788a1ba]

## 2025

- *How is this thing supposed to happen irl?*
### January – March

- Hardware design begins in KiCad. Multiple board iterations are committed along with early firmware clean‑ups and configuration handling. [70665ff, 4c22a8b]
- *My old workstation only runs up to kiCAD 6*
- By late January the "easyeda" project is created and the firmware tree moves under `firmware/` with substantial PCB files added. [20e518a]
- February and March see continual board revisions (`brd`, `brd v2`), button routing tweaks, and resistor pull-up improvements. [f36d220, dd0ebb6]

### April

- Repository reorganization and creation of the first official PCB revision `MN42-1` in EasyEDA. [174b516, f5916a7]
- *Work just a little at a time, and eventually you have a whole castle*

### May

- Firmware refactors focus on display timing, envelope follower behavior, and general stability. README updates document usage. [44e89dc, ec7edb3]
- Initial testing framework appears along with incremental fixes. [ed357da, 6a4e54a]
- *Getting onto breadboards for timing testing/display/midi/etc.*

### June

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
- *^^^ see what I mean? I am tyring to get better at documentation as I go, but this helps explain what is happening so much more than my drafts ever did*
### July

- Arpeggiator mode added with base note support and full MIDI type coverage. [850b4e9, 60abd55, e03466d]
- PlatformIO build updated to include the arpeggiator sources. [6b81ed0, 1e9a015]
- README explains new arpeggiator behavior. [09ffda2]
- Test suite expanded: biquad filter modes and EEPROM slot verification. [2716465, 9a8733b]
- Button-driven navigation simplifies test phases. [8716674]
- Helper functions unify test initialization. [bf1936f, 109a1b8]
- Variables clarified for easier maintenance. [6d769ec]

## Overview

Across roughly seven months of commits, MOARkNOBS‑42 evolved from a set of untested firmware files into a documented DIY MIDI controller complete with hardware PCB designs, test suites, and detailed usage notes. The repository now houses:

- `firmware/` – Teensy‑based C++ code with modular managers and tests.
- `hardware/` – EasyEDA design files for the `BTN_42` button matrix.
- Comprehensive README files describing features, wiring, and building instructions.

