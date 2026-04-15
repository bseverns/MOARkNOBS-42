# MOARkNOBS-42 — Assumption Ledger
*Current snapshot · Last updated: 2026-03-13*

> What we currently believe is true, what evidence we actually have, and where the remaining unknowns still need bench time.

This is a living status sheet split between source-confirmed assumptions and bench-confirmed assumptions. If a line says "pending bench refresh," that means the code path exists and the target is documented, but we are not pretending we have fresh measurement receipts when we do not.

## Source-confirmed assumptions

### 1) Firmware/App contract is locked to schema v6
- **Claim:** The current stack is built around schema version `6`.
- **Why it matters:** App/firmware drift is the fastest way to turn a safe editor into a liar.
- **Evidence:** [firmware/include/Globals.h](https://github.com/bseverns/benzknober/blob/main/firmware/include/Globals.h) defines `CONFIG_VERSION = 0x0006`, and [App/manifest_contract.js](https://github.com/bseverns/benzknober/blob/main/App/manifest_contract.js) advertises schema version `6`.
- **Current status:** Confirmed in source.
- **If false:** Treat it as a release blocker. Fix the contract or add an explicit migration path before shipping.

### 2) The default scheduler budget is intentionally tiered
- **Claim:** Core task cadence is currently 1 ms MIDI service, 10 ms serial service, 5 ms envelope updates, 50 ms LED/UI refresh, plus slower low-priority interop/streaming tasks.
- **Why it matters:** This is the runtime pacing model the rest of the firmware assumes.
- **Evidence:** [firmware/src/Globals.cpp](https://github.com/bseverns/benzknober/blob/main/firmware/src/Globals.cpp) sets the defaults, and [firmware/src/Scheduler.cpp](https://github.com/bseverns/benzknober/blob/main/firmware/src/Scheduler.cpp) registers the high/mid/low task layout that Unity now asserts directly.
- **Current status:** Confirmed in source and covered by automated tests.
- **If false:** Re-check scheduler ordering and update both the tests and the runtime docs together.

### 3) Automated coverage is broad, but not hardware-complete
- **Claim:** Unity now covers core logic plus orchestration paths, but not final board bring-up, OLED rendering on real hardware, or full physical I/O behavior.
- **Why it matters:** Contributors need to know what a green test run actually proves.
- **Evidence:** [TESTING.md](../validation/TESTING.md) maps coverage across Unity, Node tests, the system runner, and manual sketches.
- **Current status:** Confirmed in docs and build configuration.
- **If false:** Update the coverage map immediately; this one should never be guesswork.

### 4) Persistence and recovery are designed around redundancy
- **Claim:** Config data relies on mirrored/backup EEPROM regions plus recovery messaging rather than a single blind write path.
- **Why it matters:** The instrument needs to survive bad writes and partial corruption without bricking the rig.
- **Evidence:** [firmware/include/Globals.h](https://github.com/bseverns/benzknober/blob/main/firmware/include/Globals.h) defines the mirrored EEPROM layout, and [firmware/src/ConfigManager.cpp](https://github.com/bseverns/benzknober/blob/main/firmware/src/ConfigManager.cpp) contains the recovery/migration logic.
- **Current status:** Confirmed in source; broader soak validation still benefits from real hardware.
- **If false:** Stop treating EEPROM recovery as trustworthy until the layout and boot-path logic agree again.

## Bench assumptions still needing fresh receipts

### 5) Latency stays in the "feels playable" window
- **Claim:** Median knob-to-MIDI/OSC latency should land around 3-5 ms with p95 at or below 10 ms.
- **Why it matters:** Latency is not cosmetic on this instrument; it is part of the playing feel.
- **Evidence target:** [`docs/bench/latency/method.md`](../bench/latency/method.md) defines the measurement process and acceptance threshold.
- **Current status:** Target documented, pending fresh bench refresh on current hardware.
- **If false:** Re-profile scheduler load, serial throughput, and ISR work before adding new runtime complexity.

### 6) Idle ADC noise stays below the shimmer threshold
- **Claim:** Resting control noise should remain at or below 2 LSB RMS.
- **Why it matters:** Anything noisier turns "hands off" into fake input.
- **Evidence target:** [`docs/bench/noise/method.md`](../bench/noise/method.md) defines the capture process and acceptance threshold.
- **Current status:** Target documented, pending fresh bench refresh on current hardware.
- **If false:** Revisit analog filtering, smoothing, wiring practice, and power integrity before blaming UI code.

### 7) Physical power/wiring discipline is still part of correctness
- **Claim:** Stable 5 V power, short data runs, and sane grounding remain required for predictable LED + control behavior.
- **Why it matters:** The firmware can only be as honest as the board it is sitting on.
- **Evidence:** [BuildersHandbook.md](../getting-started/BuildersHandbook.md), [PinMap.md](PinMap.md), and the bench docs capture the intended wiring and measurement ritual.
- **Current status:** Design intent documented; prototype-fab validation is still ahead of us.
- **If false:** Treat it as a hardware revision issue, not just a firmware polish task.

## “Never Do” guardrails

- No telemetry or hidden data capture.
- No keystroke emulation without an explicit firmware build flag.
- No vendor-locked software requirement for core controller behavior.
- No undocumented mappings; every control should remain legible in the parameter map or UI.

## Quick bench checklist

1. Boot the board and confirm the reported firmware/build identity is the one you expect.
2. Spin adjacent controls and confirm there is no obvious cross-talk or idle shimmer.
3. Stress one control and confirm the runtime/DAW stays responsive.
4. Save and reload a profile or mapping path to confirm persistence still behaves.
5. If this is a performance-facing change, capture latency/noise receipts instead of relying on memory.
