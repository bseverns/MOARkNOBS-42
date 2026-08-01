#ifndef MN42_PROTOCOL_SCENE_STORAGE_H
#define MN42_PROTOCOL_SCENE_STORAGE_H

#include <array>
#include <cstddef>
#include <cstdint>

#include "Globals.h"
#include "MIDITypes.h"

namespace SceneStorage {
// SceneStorage is the "capture and restore whole machine state" submachine.
//
// `ConfigState` is the portable snapshot format used by both named scenes and
// the single macro scratch slot.

struct SceneInfo {
    uint8_t slot = 0;
    char name[16] = {0};
    bool available = false;
};

struct ConfigState {
    uint16_t version = 1;
    std::array<uint8_t, NUM_POTS> potChannels{};
    std::array<uint8_t, NUM_POTS> potCCNumbers{};
    std::array<MIDISlot, NUM_SLOTS> slots{};
    uint8_t argEnabled = 0;
    uint8_t argMethod = 0;
    uint8_t argSourceA = 0;
    uint8_t argSourceB = 1;
    uint8_t envelopeMode = 0;
    uint8_t ledBrightness = 255;
    uint8_t ledMode = 0;
    uint8_t ledR = 0;
    uint8_t ledG = 0;
    uint8_t ledB = 0;
    uint8_t filterType = 0;
    float filterFrequency = 20.0f;
    float filterQ = 1.0f;
    std::array<float, NUM_ENVELOPES> baselines{};
};

struct SceneEntry {
    char name[16] = {0};
    ConfigState state{};
};

constexpr uint8_t kSceneSlotCount = 6;

// Keep the persistence layout visible to storage and manifest code. These
// record sizes include the ARM alignment padding used by the on-disk structs
// in SceneStorage.cpp; the static_asserts beside those structs keep this
// contract honest if ConfigState changes.
constexpr size_t kMacroStorageAddress = EEPROM_PROFILE_MODULATION_START(NUM_PROFILES);
constexpr size_t kMacroRecordBytes = sizeof(ConfigState) + 8;
constexpr size_t kSceneStorageBase = kMacroStorageAddress + kMacroRecordBytes;
constexpr size_t kSceneRecordBytes = sizeof(ConfigState) + 24;
constexpr size_t kRequiredStorageBytes =
    kSceneStorageBase + static_cast<size_t>(kSceneSlotCount) * kSceneRecordBytes;

constexpr uint8_t sceneCapacityForStorage(size_t bytes) {
    if (bytes <= kSceneStorageBase) return 0;
    const size_t available = (bytes - kSceneStorageBase) / kSceneRecordBytes;
    return static_cast<uint8_t>(available < kSceneSlotCount ? available : kSceneSlotCount);
}

constexpr bool macroFitsInStorage(size_t bytes) {
    return bytes >= kSceneStorageBase;
}

// Named scene slots.
uint8_t listScenes(SceneInfo *scenes, size_t capacity);
bool saveSceneSlot(uint8_t slot, const ConfigState &state, const char *name);
bool loadSceneSlot(uint8_t slot, SceneEntry &entry);
bool sceneSlotAvailable(uint8_t slot);

// Single scratch macro snapshot.
bool macroSnapshotAvailable();
bool loadMacroSnapshot(ConfigState &state);
bool saveMacroSnapshot(const ConfigState &state);

// Full live-state capture/apply helpers shared by scenes and macros.
ConfigState captureConfigState();
bool applyConfigState(const ConfigState &state, bool persist);
} // namespace SceneStorage

#endif // MN42_PROTOCOL_SCENE_STORAGE_H
