# Project History

This file summarizes the development of the MOARkNOBS-42 project based on commit history.

## 2024

- **December 10:** Initial commit introduces the firmware source tree with modules for button scanning, MIDI handling, display control, and EEPROM support. [58ef040]
- **Mid December:** Early work on display management and multiplexed button matrix ("mux management" and "display/managers").
- **Late December:** Adds EEPROM and USB MIDI functionality, debounce logic, and second revision of the display code. [dbf3c21, 788a1ba]

## 2025

### January – March

- Hardware design begins in KiCad. Multiple board iterations are committed along with early firmware clean‑ups and configuration handling. [70665ff, 4c22a8b]
- By late January the "easyeda" project is created and the firmware tree moves under `firmware/` with substantial PCB files added. [20e518a]
- February and March see continual board revisions (`brd`, `brd v2`), button routing tweaks, and resistor pull-up improvements. [f36d220, dd0ebb6]

### April

- Repository reorganization and creation of the first official PCB revision `MN42-1` in EasyEDA. [174b516, f5916a7]

### May

- Firmware refactors focus on display timing, envelope follower behavior, and general stability. README updates document usage. [44e89dc, ec7edb3]
- Initial testing framework appears along with incremental fixes. [ed357da, 6a4e54a]

### June

- Early June introduces unit tests for the BiquadFilter and bug fixes around fade animations. [b050f4f, 69fd0db]
- **June 17:** Tagged as `firmware1.0`, marking a stable baseline before adding more features. [04f6b72]
- Following this, MIDI capabilities expand significantly with new message types and slot verification. [6cef527, 0e5c9d9]
- **June 23:** MIDI type combos and filter tuning feedback are implemented, and documentation is expanded. [2615616, ab8078a]
- Extensive cleanup on June 25 consolidates pin assignments, removes old files, adds the MIT license, and updates hardware docs. [bfbfbaf, cb39705]
- Additional DSP tests and control button wiring verification land on June 26. [b703f1f, 1a2b239]
- Late June brings documentation improvements describing envelope filters, ARG mode, and exhaustive testing approaches. [43b1d33, b14ef44]
- **June 28:** Final `readme tweaks` commit wraps up the documented state of the project. [8ed5568]

## Overview

Across roughly seven months of commits, MOARkNOBS‑42 evolved from a set of untested firmware files into a documented DIY MIDI controller complete with hardware PCB designs, test suites, and detailed usage notes. The repository now houses:

- `firmware/` – Teensy‑based C++ code with modular managers and tests.
- `hardware/` – EasyEDA design files for the `BTN_42` button matrix.
- Comprehensive README files describing features, wiring, and building instructions.

