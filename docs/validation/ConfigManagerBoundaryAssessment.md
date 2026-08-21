# ConfigManager Boundary Assessment

> **Doc class:** Architecture assessment. Snapshot date: 2026-08-21. This document recommends boundaries; it does not
> authorize persistence-format or wire-contract changes.

## Decision

Keep `ConfigManager` as the firmware's live-configuration and persistence façade, but stop adding protocol,
serialization, migration-format, and test-runner responsibilities to it.

The current split is useful but incomplete. `SchemaMigration.cpp`, `ProfileStorage.cpp`, and
`ProfileModulationStorage.cpp` separate some implementation, while the public class and main translation unit still span
about 85 declared methods and multiple unrelated dependency directions.

Recommended sequence:

1. remove dead/unused API and move embedded system-test bodies out of the production translation unit;
2. extract deprecated command parsing and schema generation from the persistence class;
3. move profile payload types into a dependency-light header;
4. isolate legacy profile decoding/migration behind a storage-facing codec;
5. leave core live-state and primary/backup persistence in `ConfigManager` until tests prove a narrower façade is useful.

Do not split setters/getters into one-file-per-feature modules. That would create more navigation without reducing
authority or coupling.

## Current responsibility map

| Responsibility | Current home | Boundary fit | Decision |
| --- | --- | --- | --- |
| Live slot and pot configuration | `ConfigManager` | Core responsibility | Keep |
| Primary/backup config persistence and CRC | `ConfigManager.cpp` | Core responsibility | Keep initially |
| Storage backend selection/test override | `ConfigManager.cpp` | Persistence infrastructure | Keep as façade seam |
| Profile block save/load orchestration | `ConfigManager.cpp` | Core persistence orchestration | Keep, delegate decoding |
| Profile sanitization and CRC | `ProfileStorage.cpp` | Good extracted boundary | Keep |
| Profile modulation codec | `ProfileModulationStorage.cpp` | Good extracted boundary | Keep |
| Slot/scene/storage schema migration | `SchemaMigration.cpp` | Cohesive but class-private | Keep physical split; improve test seam later |
| Six generations of legacy profile structs and conversion | top of `ConfigManager.cpp` | Migration concern inside core | Extract |
| JSON schema generation | `ConfigManager::makeSchema()` | Host contract, not persistence | Extract |
| Legacy JSON-like serialization | `ConfigManager::serializeAll()` | Unused serialization concern | Remove if guards confirm no external ABI requirement |
| Deprecated command parsing | `ConfigManager::handleCommand()` | Protocol concern with runtime/global effects | Extract |
| EF runtime refresh helper | free `saveSlotEfSettings()` in `ConfigManager.cpp` | Cross-layer orchestration | Move toward UI/runtime or slot service |
| System-test function bodies | tail of `ConfigManager.cpp` | Test concern in production source | Move to `firmware/system_test/` |
| Screensaver declarations | `ConfigManager.h` | Display concern; no definitions or callers | Remove |

## Why the class has regrown

The earlier decomposition moved slot/storage migration and profile sanitization out, but three large seams remained:

- legacy profile versions 1–6, their CRC functions, and conversion helpers occupy the first major block of
  `ConfigManager.cpp`;
- `loadProfileSettings()` contains version dispatch and repeated migration assembly;
- schema generation, legacy serialization, and deprecated protocol parsing occupy the final major block.

New persistence features therefore still tend to modify `ConfigManager.h` and `ConfigManager.cpp`, even when the feature
belongs to host protocol or migration history.

## Dependency direction problems

### The public header exports too much context

`ConfigManager.h` currently defines profile payload structs, EEPROM layout constants, migration/recovery enums, and the
manager API. It includes `Arpeggiator.h`, `Globals.h`, `PotentiometerManager.h`, Arduino/EEPROM, FastLED, STL maps, and
STL vectors. Many consumers include this full surface merely to read a slot or pass a `ProfileData` value.

The clearest reduction is a `ProfileTypes.h` containing only packed profile payloads, limits, versions, and static size
assertions. `ProfileStorage.h`, mode/profile protocol code, and migration code could then depend on profile types without
depending on the manager class.

### Protocol points back into persistence

`ProtocolDispatch` falls through to `ConfigManager::handleCommand()`. That method parses deprecated serial commands,
logs protocol responses, calibrates global envelope followers, mutates ARG state, and writes every slot. This makes the
persistence class a hidden protocol handler and gives it runtime-global dependencies.

Replace the fallback with a focused handler such as:

```cpp
bool handleLegacyConfigCommand(const String &command, ConfigManager &config);
```

The handler belongs under `firmware/src/protocol/`; `ConfigManager` should expose operations, not parse transport text.

### Profile helpers depend back on the manager header

`ProfileStorage.h` includes `ConfigManager.h` only to obtain profile types. This reverses the intended relationship:
ConfigManager should depend on a profile codec and its types, while the codec should not depend on the manager façade.

Moving profile types first creates the correct direction:

```text
ProfileTypes <- ProfileStorage/ProfileMigration <- ConfigManager <- Protocol/Modes/UI
```

## Extraction candidates

### 1. Legacy protocol handler — extract first

**Why:** narrow surface, obvious ownership, no storage-format change, and existing protocol-dispatch tests.

Move `CAL_ENVS`, deprecated filter commands, and deprecated ARG-pair commands from
`ConfigManager::handleCommand()` to `protocol/LegacyConfigCommands.cpp`. Update `ProtocolDispatch` to call the focused
handler directly.

Acceptance boundaries:

- exact command recognition and response text remain unchanged;
- malformed commands remain handled rather than falling through to “unknown command”;
- persistent vs live write behavior remains unchanged;
- protocol tests cover every retained deprecated command before/after the move.

### 2. Schema generation — extract second

**Why:** schema JSON is a host contract and a heap-audit hotspot, not persistence behavior.

Move the schema builder to `protocol/ConfigSchema.cpp` with a free API. Keep schema keyword coverage and App/firmware
contract guards authoritative. This creates the right place for later schema caching or fixed-output work without
growing ConfigManager.

`serializeAll()` has no production or test caller. Prefer deleting it after a compatibility check rather than moving
dead code.

### 3. Profile types and legacy profile codec — extract third

**Why:** largest coherent reduction and strongest dependency improvement, but persistence compatibility makes it higher
risk.

Proposed pieces:

- `ProfileTypes.h`: current packed profile types, version, route limits, and size assertion;
- `ProfileMigration.h/.cpp`: private legacy structs/CRCs and a function that reads/decodes one supported version;
- existing `ProfileStorage`: current-version sanitization and CRC.

Suggested boundary:

```cpp
enum class ProfileDecodeResult {
    Success,
    InvalidId,
    InsufficientStorage,
    UnsupportedVersion,
    ChecksumMismatch,
    ReadFailure,
};

ProfileDecodeResult decodeStoredProfile(
    StorageBackend &storage,
    uint16_t base,
    ProfileData &profile);
```

`ConfigManager::loadProfileSettings()` would validate the profile ID/address, delegate decoding, and preserve its
existing boolean API until consumers are ready for richer diagnostics.

Acceptance boundaries:

- byte layouts and version constants do not change;
- versions 1–7 retain explicit fixture coverage;
- corrupt CRC, unsupported version, undersized storage, and high-index profile behavior stay covered;
- no migration path writes storage during a read-only load unless that behavior is already contractual.

The Unity profile suite now has explicit byte-layout and checksum fixtures for legacy versions 1–6, plus current-version
coverage. The prerequisite coverage for extracting the decoder is therefore in place; the extraction itself remains a
separate persistence-sensitive change.

### 4. Test bodies — relocate independently

Three system-test bodies are compiled at the tail of `ConfigManager.cpp` under `UNIT_TEST` or `FULL_SYSTEM_COMBINED`.
Move them into `firmware/system_test/test_config_manager.cpp`, where the runner already declares and calls them. This
reduces production-source conditionals without changing firmware behavior.

## What should remain together

Keep these operations in the façade during the next pass:

- slot/pot live state and validated mutation;
- primary/backup configuration load/save and recovery reporting;
- active-profile orchestration;
- storage backend override used by tests and migration;
- explicit live-versus-persisted setters until a transaction/state service replaces them coherently.

These methods share `_stored`, `slots`, recovery state, active backend selection, and persistence invariants. Splitting
them prematurely would require friendship, broad mutable state exposure, or callback plumbing without reducing risk.

## Deferred questions

- Whether the legacy ARG tuple belongs in a dedicated compatibility state object.
- Whether primary/backup config blocks and profile blocks should use one generic verified-record helper.
- Whether `saveSlotEfSettings()` belongs in UI, runtime, or a slot-configuration service.
- Whether migration methods should stop being private `ConfigManager` members and become a standalone migration engine.
- Whether callers should receive richer persistence error results instead of booleans.

Answer these through tests and failure-reporting needs, not file size.

## First implementation increment — completed

Completed on 2026-08-21:

1. added focused coverage for every command formerly handled by `ConfigManager::handleCommand()`;
2. moved that handler into `protocol/LegacyConfigCommands.*`;
3. removed `handleCommand()`, unused `serializeAll()`, and the two dead screensaver declarations from the public API;
4. moved the embedded persistence tests into `firmware/system_test/test_config_manager.cpp` and registered the orphaned
   high-index envelope assignment test;
5. added profile migration fixtures for versions 1–4, completing explicit legacy fixture coverage through version 6.

The production and full-system images compile with no persistence-layout change. The Unity image also compiles and links;
executing it still requires a connected Teensy. Schema generation and profile migration extraction remain the next two
structural boundaries.
