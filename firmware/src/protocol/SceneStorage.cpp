#include "protocol/SceneStorage.h"

#include <Arduino.h>
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "ARGMixer.h"
#include "ConfigManager.h"
#include "EfSettingsUtils.h"
#include "EnvelopeFollower.h"
#include "FirmwareState.h"
#include "Globals.h"
#include "Modes.h"
#include "Utility.h"

// SceneStorage.cpp is the whole-machine snapshot layer shared by scenes and
// the one-slot macro buffer.
//
// Reading order:
// 1. local CRC/storage helpers
// 2. full live-state capture
// 3. full live-state apply
// 4. named scene slot list/save/load helpers
// 5. macro snapshot save/load helpers

const char *envelopeModeName(uint8_t mode);
EnvelopeFollower::ARG_Method toFollowerArgMethod(ARGMethod method);

namespace {
uint16_t crc16Update(uint16_t crc, uint8_t data) {
    crc ^= static_cast<uint16_t>(data) << 8;
    for (uint8_t i = 0; i < 8; ++i) {
        if (crc & 0x8000) {
            crc = static_cast<uint16_t>((crc << 1) ^ 0x1021);
        } else {
            crc <<= 1;
        }
    }
    return crc;
}

template <typename T> uint16_t computeCrc(const T &value, size_t skipBytes = 0) {
    uint16_t crc = 0xFFFF;
    const uint8_t *bytes = reinterpret_cast<const uint8_t *>(&value);
    for (size_t i = skipBytes; i < sizeof(T); ++i) {
        crc = crc16Update(crc, bytes[i]);
    }
    return crc;
}

StorageBackend &activeStorageBackend() { return *ConfigManager::getStorageBackend(); }

template <typename T> void storageGet(int address, T &value) {
    activeStorageBackend().readBytes(address, &value, sizeof(T));
}

template <typename T> void storagePut(int address, const T &value) {
    activeStorageBackend().writeBytes(address, &value, sizeof(T));
}

struct MacroRecord {
    uint16_t version = 1;
    uint16_t crc = 0;
    uint8_t occupied = 0;
    SceneStorage::ConfigState state{};
};

struct SceneRecord {
    uint16_t version = 1;
    uint16_t crc = 0;
    uint8_t occupied = 0;
    char name[16] = {0};
    SceneStorage::ConfigState state{};
};

static_assert(sizeof(MacroRecord) == SceneStorage::kMacroRecordBytes,
              "Macro persistence layout constant is stale");
static_assert(sizeof(SceneRecord) == SceneStorage::kSceneRecordBytes,
              "Scene persistence layout constant is stale");

constexpr uint16_t kStorageVersion = 1;
constexpr uint16_t kMacroStorageAddress =
    static_cast<uint16_t>(SceneStorage::kMacroStorageAddress);
constexpr uint16_t kSceneStorageBase =
    static_cast<uint16_t>(SceneStorage::kSceneStorageBase);

uint16_t sceneSlotAddress(uint8_t slot) {
    return static_cast<uint16_t>(kSceneStorageBase + slot * sizeof(SceneRecord));
}

bool recordValid(const MacroRecord &record) {
    return record.version == kStorageVersion && record.occupied != 0 &&
           record.crc == computeCrc(record, sizeof(record.version) + sizeof(record.crc));
}

bool recordValid(const SceneRecord &record) {
    return record.version == kStorageVersion && record.occupied != 0 &&
           record.crc == computeCrc(record, sizeof(record.version) + sizeof(record.crc));
}

void copySceneName(char (&dest)[16], const char *name) {
    std::memset(dest, 0, sizeof(dest));
    if (!name || name[0] == '\0') {
        return;
    }
    const size_t len = std::min(strlen(name), sizeof(dest) - 1);
    std::memcpy(dest, name, len);
}

uint8_t normalizeEnvelopeSourceIndex(int rawSource) {
    if (rawSource >= NUM_ENVELOPES) {
        int converted = envelopeIndexFromAnalogPin(rawSource);
        rawSource = converted >= 0 ? converted : constrain(rawSource, 0, NUM_ENVELOPES - 1);
    }
    return static_cast<uint8_t>(constrain(rawSource, 0, NUM_ENVELOPES - 1));
}

void capturePotMappings(SceneStorage::ConfigState &snapshot) {
    for (uint8_t i = 0; i < configManager.getNumPots(); ++i) {
        snapshot.potChannels[i] = constrain(configManager.getPotChannel(i), 1, 16);
        snapshot.potCCNumbers[i] = constrain(configManager.getPotCCNumber(i), 0, 127);
    }
}

void captureArgAndModeState(SceneStorage::ConfigState &snapshot) {
    snapshot.argEnabled = configManager.getARGEnable();
    snapshot.argMethod = configManager.getARGMethod();
    snapshot.argSourceA =
        normalizeEnvelopeSourceIndex(static_cast<int>(configManager.getEnvelopeA()));
    snapshot.argSourceB =
        normalizeEnvelopeSourceIndex(static_cast<int>(configManager.getEnvelopeB()));
    snapshot.envelopeMode = configManager.getMode();
}

void captureLedState(SceneStorage::ConfigState &snapshot) {
    snapshot.ledBrightness = ledManager.getBrightness();
    snapshot.ledMode = static_cast<uint8_t>(configManager.getLedMode());
    const CRGB color = ledManager.getColor();
    snapshot.ledR = color.r;
    snapshot.ledG = color.g;
    snapshot.ledB = color.b;
}

void captureFilterAndBaselineState(SceneStorage::ConfigState &snapshot) {
    if (!envelopeFollowers.empty()) {
        snapshot.filterType = static_cast<uint8_t>(envelopeFollowers.front().getFilterType());
    }
    storageGet(EEPROM_FILTER_FREQ, snapshot.filterFrequency);
    storageGet(EEPROM_FILTER_Q, snapshot.filterQ);
    for (uint8_t i = 0; i < NUM_ENVELOPES; ++i) {
        snapshot.baselines[i] = envelopeConfig.baselines[i];
    }
}

void applySlotState(const SceneStorage::ConfigState &state) {
    const auto slotsState = state.slots;
    for (uint8_t i = 0; i < NUM_SLOTS; ++i) {
        MIDISlot slot = slotsState[i];
        slot.midiChannel = constrain(slot.midiChannel, 1, 16);
        slot.data1 = constrain(slot.data1, 0, 127);
        configManager.saveSlot(i, slot);
    }
}

void applyPotMappings(const SceneStorage::ConfigState &state, bool persist) {
    potChannels.clear();
    for (uint8_t i = 0; i < configManager.getNumPots(); ++i) {
        const uint8_t channel = constrain(state.potChannels[i], 1, 16);
        const uint8_t cc = constrain(state.potCCNumbers[i], 0, 127);
        configManager.setPotChannel(i, channel);
        configManager.setPotCCNumber(i, cc);
        potentiometerManager.setChannel(i, channel);
        potentiometerManager.setCCNumber(i, cc);
        potChannels.push_back(channel);
    }
    if (persist) {
        configManager.saveConfiguration();
    }
}

std::array<bool, NUM_ENVELOPES> rebuildFollowerAssignmentsFromSlots() {
    potToEnvelopeMap.clear();
    std::array<bool, NUM_ENVELOPES> followerAssigned{};
    followerAssigned.fill(false);

    for (uint8_t slotIndex = 0; slotIndex < NUM_SLOTS; ++slotIndex) {
        MIDISlot &slot = configManager.getSlot(slotIndex);
        const int followerIndex = slot.getEnvelopeFollowerIndex();
        if (followerIndex < 0 || followerIndex >= static_cast<int>(envelopeFollowers.size())) {
            continue;
        }
        potToEnvelopeMap[slotIndex] = slot.efSettings;
        followerAssigned[static_cast<size_t>(followerIndex)] = true;
        envelopeFollowers[followerIndex].setModulationTarget(
            potentiometerManager.getCCNumber(slotIndex));
        applyEfSettingsToFollower(envelopeFollowers[followerIndex], slot.efSettings,
                                  static_cast<uint8_t>(followerIndex));
    }

    return followerAssigned;
}

void applyArgAndEnvelopeModeState(const SceneStorage::ConfigState &state) {
    const uint8_t argMethod =
        static_cast<uint8_t>(constrain(state.argMethod, 0, static_cast<int>(ARGMethod::XORR)));
    const bool argEnabled = state.argEnabled != 0;
    envelopeFollowMode = argEnabled;
    configManager.setMode(state.envelopeMode);
    configManager.setARGEnable(argEnabled ? 1 : 0);
    configManager.setARGMethod(argMethod);
    configManager.setEnvelopePair(state.argSourceA, state.argSourceB);
    potentiometerManager.setArgEnvelopePair(state.argSourceA, state.argSourceB);
    updateEnvelopeModeLabel(envelopeModeName(state.envelopeMode));
}

SlotEnvelopePayload applyGlobalFollowerFilterState(const SceneStorage::ConfigState &state) {
    SlotEnvelopePayload tailPayload{};
    tailPayload.filterType = static_cast<uint8_t>(
        constrain(state.filterType, 0, static_cast<int>(EnvelopeFollower::BANDPASS)));
    tailPayload.frequency = constrain(state.filterFrequency, 20.0f, 5000.0f);
    tailPayload.q = constrain(state.filterQ, 0.5f, 4.0f);
    return configManager.persistFilterTail(tailPayload);
}

void applyFollowerRuntimeState(const SceneStorage::ConfigState &state,
                               const std::array<bool, NUM_ENVELOPES> &followerAssigned,
                               const SlotEnvelopePayload &sanitizedTail) {
    const auto followerMethod = toFollowerArgMethod(
        static_cast<ARGMethod>(constrain(state.argMethod, 0, static_cast<int>(ARGMethod::XORR))));
    const bool argEnabled = state.argEnabled != 0;
    for (uint8_t i = 0; i < envelopeFollowers.size(); ++i) {
        envelopeFollowers[i].toggleActive(followerAssigned[i]);
        envelopeFollowers[i].setARGMethod(followerMethod);
        envelopeFollowers[i].setEnvelopePair(state.argSourceA, state.argSourceB);
        envelopeFollowers[i].setMode(argEnabled ? EnvelopeFollower::ARG : EnvelopeFollower::SEF);
        envelopeFollowers[i].setFilterType(
            static_cast<EnvelopeFollower::FilterType>(sanitizedTail.filterType));
        envelopeFollowers[i].configureFilter(sanitizedTail.frequency, sanitizedTail.q);
        envelopeFollowers[i].setBaseline(state.baselines[i]);
        envelopeConfig.baselines[i] = state.baselines[i];
    }
}

void applyLedState(const SceneStorage::ConfigState &state) {
    const CRGB color(state.ledR, state.ledG, state.ledB);
    ledManager.setBrightness(state.ledBrightness);
    ledManager.setColor(color);
    configManager.saveLEDSettings(state.ledBrightness, color);
    LedMode ledMode = static_cast<LedMode>(state.ledMode);
    configManager.setLedMode(ledMode);
    ledAnimator.setMode(ledMode);
}

} // namespace

namespace SceneStorage {
// 2. Full live-state capture.
ConfigState captureConfigState() {
    ConfigState snapshot{};
    capturePotMappings(snapshot);
    snapshot.slots = configManager.getSlots();
    captureArgAndModeState(snapshot);
    captureLedState(snapshot);
    captureFilterAndBaselineState(snapshot);
    return snapshot;
}

// 3. Full live-state apply.
void applyConfigState(const ConfigState &state, bool persist) {
    StorageBackend &storage = activeStorageBackend();
    const bool transactionalPersist = persist && storage.supportsTransactions();
    if (transactionalPersist && !storage.beginTransaction()) {
        return;
    }
    applySlotState(state);
    applyPotMappings(state, persist);
    const auto followerAssigned = rebuildFollowerAssignmentsFromSlots();
    applyArgAndEnvelopeModeState(state);
    const SlotEnvelopePayload sanitizedTail = applyGlobalFollowerFilterState(state);
    applyFollowerRuntimeState(state, followerAssigned, sanitizedTail);
    applyLedState(state);

    if (persist) {
        configManager.saveEnvelopeSettings(potToEnvelopeMap, envelopeFollowers);
    }
    refreshEfVoicesFromConfig();
    if (transactionalPersist && !storage.commitTransaction()) {
        storage.abortTransaction();
    }
}

// 4. Named scene slot helpers.
uint8_t listScenes(SceneInfo *scenes, size_t capacity) {
    if (!scenes || capacity == 0) {
        return 0;
    }
    const uint8_t count = static_cast<uint8_t>(std::min<size_t>(capacity, kSceneSlotCount));
    for (uint8_t slot = 0; slot < count; ++slot) {
        scenes[slot].slot = slot;
        std::snprintf(scenes[slot].name, sizeof(scenes[slot].name), "Scene %u",
                      static_cast<unsigned>(slot + 1));
        scenes[slot].available = false;
        SceneRecord record{};
        storageGet(sceneSlotAddress(slot), record);
        if (recordValid(record)) {
            std::memcpy(scenes[slot].name, record.name, sizeof(scenes[slot].name));
            scenes[slot].name[sizeof(scenes[slot].name) - 1] = '\0';
            scenes[slot].available = true;
        }
    }
    return count;
}

bool saveSceneSlot(uint8_t slot, const ConfigState &state, const char *name) {
    if (slot >= kSceneSlotCount) {
        return false;
    }
    SceneRecord record{};
    record.version = kStorageVersion;
    record.occupied = 1;
    record.state = state;
    copySceneName(record.name, name);
    record.crc = computeCrc(record, sizeof(record.version) + sizeof(record.crc));
    StorageBackend &storage = activeStorageBackend();
    if (storage.supportsTransactions() && !storage.beginTransaction()) {
        return false;
    }
    storagePut(sceneSlotAddress(slot), record);
    SceneRecord verified{};
    storageGet(sceneSlotAddress(slot), verified);
    if (!recordValid(verified)) {
        storage.abortTransaction();
        return false;
    }
    if (!storage.supportsTransactions()) {
        return true;
    }
    const bool committed = storage.commitTransaction();
    if (!committed) storage.abortTransaction();
    return committed;
}

bool loadSceneSlot(uint8_t slot, SceneEntry &entry) {
    if (slot >= kSceneSlotCount) {
        return false;
    }
    SceneRecord record{};
    storageGet(sceneSlotAddress(slot), record);
    if (!recordValid(record)) {
        return false;
    }
    std::memset(entry.name, 0, sizeof(entry.name));
    std::memcpy(entry.name, record.name, sizeof(entry.name));
    entry.name[sizeof(entry.name) - 1] = '\0';
    entry.state = record.state;
    return true;
}

bool sceneSlotAvailable(uint8_t slot) {
    SceneRecord record{};
    storageGet(sceneSlotAddress(slot), record);
    return recordValid(record);
}

// 5. Macro snapshot helpers.
bool macroSnapshotAvailable() {
    MacroRecord record{};
    storageGet(kMacroStorageAddress, record);
    return recordValid(record);
}

bool loadMacroSnapshot(ConfigState &state) {
    MacroRecord record{};
    storageGet(kMacroStorageAddress, record);
    if (!recordValid(record)) {
        return false;
    }
    state = record.state;
    return true;
}

bool saveMacroSnapshot(const ConfigState &state) {
    MacroRecord record{};
    record.version = kStorageVersion;
    record.occupied = 1;
    record.state = state;
    record.crc = computeCrc(record, sizeof(record.version) + sizeof(record.crc));
    StorageBackend &storage = activeStorageBackend();
    if (storage.supportsTransactions() && !storage.beginTransaction()) {
        return false;
    }
    storagePut(kMacroStorageAddress, record);
    MacroRecord verified{};
    storageGet(kMacroStorageAddress, verified);
    if (!recordValid(verified)) {
        storage.abortTransaction();
        return false;
    }
    if (!storage.supportsTransactions()) {
        return true;
    }
    const bool committed = storage.commitTransaction();
    if (!committed) storage.abortTransaction();
    return committed;
}
} // namespace SceneStorage
