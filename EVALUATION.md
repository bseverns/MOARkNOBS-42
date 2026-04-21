# Forecast Hardware & Codebase Evaluation: MOARkNOBS-42

## 1. Forecast Hardware Usability & Constraints
The **MOARkNOBS-42** operates on a **Teensy 4.0** (NXP i.MX RT1062, 600MHz, 1MB RAM), an extremely capable microcontroller for real-time DSP and MIDI operations. Given the hardware, the codebase is mostly aligned with the constraints and usability limits, but there are projected gaps when scaling or considering long-term performance:

- **RAM Overhead vs Usability:** The board has 1MB RAM, meaning 64-item `std::queue<String>` and dynamic JSON buffers are easily swallowed. However, continuous allocations will eventually hit memory fragmentation ceilings even with large heaps.
- **EEPROM Churn:** `ConfigManager` persists settings. Rapid parameter updates without debouncing or rate-limiting EEPROM writes could degrade EEPROM lifespan. Teensy 4.0 emulates EEPROM over flash, so block erasures matter.
- **Hardware Diagnostics:** The `diagnosticMode` provides vital real-time visibility on the OLED for drops and overruns. This is an excellent usability feature for live performers and builders.

## 2. Memory Leaks & Heap Fragmentation
While there are no direct `malloc()`/`free()` memory leaks spotted in the application layer, the firmware makes extensive use of STL containers dynamically reallocated in high-frequency paths.

### **Identified Gaps:**
1. **~~TaskScheduler Update Loop~~ (RESOLVED):** `dueTaskIndices` and `finished` are now permanent `std::vector` class members with pre-reserved capacity (`kReservedTaskCapacity = 96`). The constructor calls `.reserve()` and `update()` uses `.clear()` on each tick, reusing their capacity buffers without heap allocations. The original per-tick local-vector allocation has been eliminated.
   - **Remaining minor item:** One-shot task removal still uses `tasks.erase()` in a reverse loop, which is O(n) per erasure within a vector. For the current task counts (~10–20 registered tasks) this is negligible, but a swap-and-pop or `std::remove_if` + `erase` pattern would be more efficient if task counts ever grow significantly.
2. **CommandQueue and Protocol Parsing:** `pollSerialInput` uses `String` to capture serial buffers and pushes them to `std::queue<String>`.
   - **Impact:** Strings continuously dynamically allocate.
   - **Tune:** While acceptable given the Teensy's RAM size, keeping strings trimmed and moving towards a fixed-size `char` ring-buffer in the future would eliminate this minor fragmentation source.

## 3. Data Flow & Cache Management
- **Deterministic Boot and Globals:** `firmware_main.cpp` relies on static/global allocations for its Managers (`ButtonManager`, `LEDManager`, etc.). This guarantees predictable deterministic boot sequence and ensures they are in stable memory (BSS/Data sections).
- **Callback Captures (`std::function`):** E.g., `scheduleNoteOnOff` captures variables into a lambda. The capture size is 3-4 machine words (a pointer to `MIDIHandler` plus some `uint8_t` variables), safely fitting into `std::function`'s Small Object Optimization (SOO) buffer without forcing an allocation.
- **Data Transport Arbitration:** `MIDIHandler` successfully queues and defers MIDI traffic via `enqueueSerialMessage()`. This correctly decouples ISR/time-sensitive USB/Hardware serial writes from blocking DSP operations.

## 4. Proposed Tunes & Action Items
1. **~~Fix `TaskScheduler` Fragmentation~~ (DONE):** Vectors promoted to class members; capacity pre-reserved in the constructor. Reverse-erase loop replaced with O(n) `std::remove_if` + `erase` pattern; `finished` vector eliminated.
2. **EEPROM Safety:** Ensure `saveConfiguration` and profile saving calls aren't triggered unintentionally in high-frequency event loops without dirty flags. (Codebase currently uses valid dirty checking).
3. **Queue Overflows:** The 64-item max limit in `pollSerialInput` successfully drops the oldest strings gracefully, preserving interactivity under DOS/Overload conditions.
4. **~~ConfigManager Decomposition~~ (DONE):** The 1567-line monolith has been split into three focused translation units:
   - `ConfigManager.cpp` (1111 lines) — Core EEPROM persistence, accessors, serialization, command handling.
   - `SchemaMigration.cpp` (525 lines) — Legacy slot layout upgrades (v3→v4→v5), slot-arena sanitization, profile block wipes.
   - `ProfileStorage.cpp` (126 lines) — Profile payload sanitization, CRC computation, EF settings clamping.
5. **~~DRY `platformio.ini` Build Filters~~ (DONE):** Extracted the ~18-line shared module list duplicated across 5 hardware test envs into a `[core_modules]` section. Each test env now references `${core_modules.build_src_filter}` and appends only its unique entry point and extras. Reduced from 393 → 339 lines, eliminating the need to edit 5+ places when adding a new shared module.
