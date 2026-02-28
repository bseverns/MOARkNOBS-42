# Forecast Hardware & Codebase Evaluation: MOARkNOBS-42

## 1. Forecast Hardware Usability & Constraints
The **MOARkNOBS-42** operates on a **Teensy 4.0** (NXP i.MX RT1062, 600MHz, 1MB RAM), an extremely capable microcontroller for real-time DSP and MIDI operations. Given the hardware, the codebase is mostly aligned with the constraints and usability limits, but there are projected gaps when scaling or considering long-term performance:

- **RAM Overhead vs Usability:** The board has 1MB RAM, meaning 64-item `std::queue<String>` and dynamic JSON buffers are easily swallowed. However, continuous allocations will eventually hit memory fragmentation ceilings even with large heaps.
- **EEPROM Churn:** `ConfigManager` persists settings. Rapid parameter updates without debouncing or rate-limiting EEPROM writes could degrade EEPROM lifespan. Teensy 4.0 emulates EEPROM over flash, so block erasures matter.
- **Hardware Diagnostics:** The `diagnosticMode` provides vital real-time visibility on the OLED for drops and overruns. This is an excellent usability feature for live performers and builders.

## 2. Memory Leaks & Heap Fragmentation
While there are no direct `malloc()`/`free()` memory leaks spotted in the application layer, the firmware makes extensive use of STL containers dynamically reallocated in high-frequency paths.

### **Identified Gaps:**
1. **TaskScheduler Update Loop:** `TaskScheduler::update()` dynamically allocates `std::vector<std::function<void()>> dueCallbacks` and `std::vector<size_t> finished` *on every single tick*.
   - **Impact:** With a 600MHz CPU and 1MB RAM, it won't crash instantly, but since `update()` is ticked continuously, allocating and deallocating these vectors (and their inner `std::function` objects if they exceed the SSO buffer) leads to severe heap fragmentation over long operational periods.
   - **Tune:** Make `dueCallbacks` and `finished` permanent `std::vector` members of the `TaskScheduler` class and call `.clear()` on each update. This reuses their capacity buffer and avoids heap allocations.
2. **CommandQueue and Protocol Parsing:** `pollSerialInput` uses `String` to capture serial buffers and pushes them to `std::queue<String>`.
   - **Impact:** Strings continuously dynamically allocate.
   - **Tune:** While acceptable given the Teensy's RAM size, keeping strings trimmed and moving towards a fixed-size `char` ring-buffer in the future would eliminate this minor fragmentation source.

## 3. Data Flow & Cache Management
- **Deterministic Boot and Globals:** `firmware_main.cpp` relies on static/global allocations for its Managers (`ButtonManager`, `LEDManager`, etc.). This guarantees predictable deterministic boot sequence and ensures they are in stable memory (BSS/Data sections).
- **Callback Captures (`std::function`):** E.g., `scheduleNoteOnOff` captures variables into a lambda. The capture size is 3-4 machine words (a pointer to `MIDIHandler` plus some `uint8_t` variables), safely fitting into `std::function`'s Small Object Optimization (SOO) buffer without forcing an allocation.
- **Data Transport Arbitration:** `MIDIHandler` successfully queues and defers MIDI traffic via `enqueueSerialMessage()`. This correctly decouples ISR/time-sensitive USB/Hardware serial writes from blocking DSP operations.

## 4. Proposed Tunes & Action Items
1. **Fix `TaskScheduler` Fragmentation:** Move local `dueCallbacks` and `finished` vectors to class fields to prevent per-tick allocation overhead. (Implementing now).
2. **EEPROM Safety:** Ensure `saveConfiguration` and profile saving calls aren't triggered unintentionally in high-frequency event loops without dirty flags. (Codebase currently uses valid dirty checking).
3. **Queue Overflows:** The 64-item max limit in `pollSerialInput` successfully drops the oldest strings gracefully, preserving interactivity under DOS/Overload conditions.
