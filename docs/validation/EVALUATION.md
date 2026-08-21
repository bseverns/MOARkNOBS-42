# Forecast Hardware & Codebase Evaluation: MOARkNOBS-42

> **Status snapshot:** 2026-08-21. This evaluation distinguishes implemented safeguards from remaining investigations.
> File lengths are observations, not completion criteria; behavior and test evidence decide whether an item is closed.

## 1. Hardware usability and constraints

MOARkNOBS-42 runs on a Teensy 4.0 (NXP i.MX RT1062, 600 MHz, 1 MB RAM). The hardware remains capable for the current
real-time MIDI, control, display, and modulation workload, with three long-term constraints worth tracking:

- **Heap stability:** The serial ingress queue no longer allocates dynamically, but Arduino `String`, ArduinoJson, and
  some STL-backed storage remain in protocol assembly, serialization, display, and setup/control paths. These require a
  path-by-path audit before making a blanket “allocation-free runtime” claim.
- **Flash-backed persistence:** Configuration and profile writes must stay operator-triggered or dirty/debounce gated.
  Current code has those safeguards, but a quantified write-rate/endurance receipt would turn inspection confidence into
  evidence.
- **Hardware diagnostics:** Diagnostic mode exposes drops, overruns, and status on the OLED. This remains valuable for
  live performers, builders, and future endurance testing.

## 2. Memory and heap behavior

No direct `malloc()`/`free()` ownership leak is currently identified in the application layer. The remaining question is
fragmentation and peak allocation behavior, especially around large protocol/configuration payloads.

### Completed: TaskScheduler update loop

`TaskScheduler` keeps `dueTaskIndices` as a permanent member, reserves `kReservedTaskCapacity`, and clears/reuses the
buffer on each update. Completed one-shot tasks are removed with one O(n) `std::remove_if` + `erase` sweep. The old
per-tick temporary vectors and reverse-erase loop are gone.

Current source: `firmware/src/Utility.cpp`, `firmware/include/Utility.h`.

### Completed: fixed serial command queue

`pollSerialInput` now feeds a fixed-capacity `char` ring buffer rather than `std::queue<String>`:

- capacity: 192 complete serial lines
- storage: `DMAMEM`, keeping the large queue in RAM2
- line length: bounded by `SERIAL_BUFFER_SIZE`
- overflow policy: reject the newest complete line and preserve already queued frame fragments
- overlong-line policy: discard through the next newline instead of dispatching a truncated tail
- per-pass ingress budget: 256 serial bytes

This removes the evaluation's original high-frequency queue-allocation concern. The previous description of a 64-item
queue that dropped the oldest `String` is obsolete.

Current source: `firmware/src/CommandQueue.cpp`.

### Remaining investigation: dynamic payload construction

The fixed ingress queue does not make the entire protocol path allocation-free. Remaining areas to inventory include:

- the persistent, pre-reserved command `String` used by `processCommandQueue`
- `Utility::BulkConfigAssembler` payload and checksum strings
- configuration/schema serialization in `ConfigManager`
- ArduinoJson document capacity and lifetime across protocol handlers
- display/status strings and diagnostic/log formatting
- `std::function` callback storage when a capture exceeds the implementation's small-object buffer

The next useful result is an evidence table naming allocation site, execution frequency, maximum size, reuse strategy,
and whether it runs during steady-state performance. Convert code only where that evidence shows meaningful runtime risk.

That source/build inventory is now captured in
[Firmware Heap Allocation Audit](docs/validation/HeapAllocationAudit.md). It identifies connected WebSerial telemetry as
the first bounded hardening target, followed by chunked-read and bulk-Apply burst allocation.

## 3. Persistence safety

### Implemented safeguards

- On-device configuration mode saves only when `_onDeviceConfigModeDirty` is true.
- Filter tuning persistence uses a dirty flag, idle delay, and minimum write interval.
- Profile/configuration writes are attached to explicit save/apply/reset commands or deferred profile-save requests.
- The storage backend uses update-style writes and primary/backup validation rather than blind continuous rewrites.

### Remaining evidence

Code inspection shows appropriate gating, but the repository still needs a reproducible persistence-endurance argument:

1. enumerate every runtime call site that can reach configuration, profile, scene, macro, and filter-tail persistence;
2. classify each as boot repair, explicit operator action, debounced edit, transaction Apply, or unintended periodic path;
3. calculate the maximum expected write frequency for the debounced paths;
4. add counters or a test seam if existing diagnostics cannot observe writes by storage region;
5. publish an automated stress result or dated bench receipt with the backend, duration, operation count, recovery
   behavior, and pass/fail boundary.

This is evidence work, not a claim that current firmware is rapidly wearing flash.

## 4. Data flow and cache management

- **Deterministic boot and globals:** Core managers remain static/global allocations, giving the primary runtime objects
  stable storage and deterministic construction/boot behavior.
- **Callback captures:** Existing scheduled callback captures appear small, but small-object optimization is an
  implementation property rather than a portable guarantee. The allocation audit should verify actual capture sizes or
  avoid relying on an undocumented threshold for hot-path callbacks.
- **Transport arbitration:** MIDI and serial work is queued/deferred so time-sensitive scanning and DSP work do not
  perform blocking host transport writes directly.

## 5. Structural maintenance

### Completed: ConfigManager decomposition

Migration and profile-sanitization helpers remain separated into:

- `firmware/src/ConfigManager.cpp`
- `firmware/src/SchemaMigration.cpp`
- `firmware/src/ProfileStorage.cpp`

As of this snapshot they are approximately 1,878, 869, and 181 lines. Those counts have grown since the original split;
the split is still real, but `ConfigManager.cpp` and `SchemaMigration.cpp` should be reassessed by responsibility before
adding more behavior. A line-count-only extraction is not recommended.

The responsibility and dependency analysis is now recorded in
[ConfigManager Boundary Assessment](docs/validation/ConfigManagerBoundaryAssessment.md). It recommends keeping the
persistence façade while extracting deprecated protocol handling first, then schema generation, profile types, and
legacy profile decoding in independently verified increments.

### Completed: shared PlatformIO module filters

`firmware/platformio.ini` defines `[core_modules]`, and broad full-system/unified/biquad/EEPROM test environments reuse
`${core_modules.build_src_filter}`. Narrow environments such as slot verification retain intentionally smaller filters.
The file has grown as environments were added; total line count does not mean the shared-filter work regressed.

## 6. Remaining action plan

1. **~~Audit dynamic allocation~~ (DONE):** The runtime inventory identifies connected WebSerial telemetry as the
   highest-frequency allocation path.
2. **Harden one proven hotspot:** Prefer fixed/preallocated storage or explicit reserve/reuse where measurements or code
   frequency show a real fragmentation risk.
3. **Prove persistence write discipline:** Add a call-site ledger, observable write counts, and a stress/bench receipt.
4. **~~Reassess ConfigManager boundaries~~ (DONE):** The boundary assessment identifies protocol handling as the first
   safe extraction and legacy profile decoding as the highest-value structural extraction after fixture coverage.

The scheduler and serial command queue items are closed. Reopening them requires new failing evidence, not their obsolete
descriptions from earlier evaluation snapshots.
