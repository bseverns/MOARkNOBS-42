// SchemaMigration.cpp — EEPROM schema migration extracted from ConfigManager.
//
// These ConfigManager member functions handle slot-arena sanitization, legacy
// layout upgrades (v3→v4→v5), profile block wipes, and default seeding.
// They run at boot during ConfigManager::begin() and during factory resets.
//
// Splitting them out keeps the hot-path persistence code in ConfigManager.cpp
// small while making migrations independently auditable and testable.

#include "ConfigManager.h"
#include "ProfileStorage.h"
#include "SchemaMigrationLayout.h"
#include "EnvelopeFollower.h"
#include "ARGMixer.h"
#include "protocol/SceneStorage.h"
#include "storage/EepromStorageBackend.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstring>
#include "Log.h"

// ---------- storage access shorthand (mirrors ConfigManager.cpp) ----------
namespace {

uint8_t storageRead(int address) { return ConfigManager::getStorageBackend()->read(address); }

void storageUpdate(int address, uint8_t value) {
    ConfigManager::getStorageBackend()->update(address, value);
}

template <typename T> void storageGet(int address, T &value) {
    ConfigManager::getStorageBackend()->readBytes(address, &value, sizeof(T));
}

template <typename T> bool storagePut(int address, const T &value) {
    return ConfigManager::getStorageBackend()->writeBytes(address, &value, sizeof(T));
}

template <typename T> bool storagePutVerified(int address, const T &value) {
    StorageBackend *storage = ConfigManager::getStorageBackend();
    if (!storage->contains(address, sizeof(T)) ||
        !storage->writeBytes(address, &value, sizeof(T))) {
        return false;
    }
    T restored{};
    storage->readBytes(address, &restored, sizeof(T));
    return std::memcmp(&restored, &value, sizeof(T)) == 0;
}

bool storageUpdateVerified(int address, uint8_t value) {
    StorageBackend *storage = ConfigManager::getStorageBackend();
    return storage->contains(address) && storage->update(address, value) &&
           storage->read(address) == value;
}

constexpr uint16_t kLegacyConfigVersion = 0x0003;
constexpr float kMinFilterFrequency = 20.0f;
constexpr float kMaxFilterFrequency = 5000.0f;
constexpr float kMinFilterQ = 0.5f;
constexpr float kMaxFilterQ = 4.0f;

struct LegacyMIDISlotV6 {
    MIDIMessageType type = MIDIMessageType::OFF;
    uint8_t midiChannel = 1;
    uint8_t data1 = 0;
    bool active = false;
    uint8_t arpNote = 0;
    uint8_t sysexLength = 0;
    MIDISlot::EfSettings efSettings{};
    std::array<uint8_t, SysExTemplate::kMaxLength> sysexTemplate{};
    MIDISlot::EfRuntime ef{};
    SlotARGConfig arg{};
};

static_assert(sizeof(MIDISlot) == sizeof(LegacyMIDISlotV6) + sizeof(SlotLfoConfig),
              "Schema-6 slot size no longer matches the migration contract");
static_assert(offsetof(LegacyMIDISlotV6, efSettings) == offsetof(MIDISlot, efSettings),
              "Legacy v6 EF layout drifted");
static_assert(offsetof(LegacyMIDISlotV6, arg) == offsetof(MIDISlot, arg),
              "Legacy v6 ARG layout drifted");

struct LegacyConfigStateV6 {
    uint16_t version = 1;
    std::array<uint8_t, NUM_POTS> potChannels{};
    std::array<uint8_t, NUM_POTS> potCCNumbers{};
    std::array<LegacyMIDISlotV6, NUM_SLOTS> slots{};
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

struct LegacyMacroRecordV6 {
    uint16_t version = 1;
    uint16_t crc = 0;
    uint8_t occupied = 0;
    LegacyConfigStateV6 state{};
};

struct LegacySceneRecordV6 {
    uint16_t version = 1;
    uint16_t crc = 0;
    uint8_t occupied = 0;
    char name[16] = {0};
    LegacyConfigStateV6 state{};
};

struct MigratedMacroRecord {
    uint16_t version = 1;
    uint16_t crc = 0;
    uint8_t occupied = 0;
    SceneStorage::ConfigState state{};
};

struct MigratedSceneRecord {
    uint16_t version = 1;
    uint16_t crc = 0;
    uint8_t occupied = 0;
    char name[16] = {0};
    SceneStorage::ConfigState state{};
};

static_assert(sizeof(MigratedMacroRecord) == SceneStorage::kMacroRecordBytes,
              "Migrated macro layout drifted from SceneStorage");
static_assert(sizeof(MigratedSceneRecord) == SceneStorage::kSceneRecordBytes,
              "Migrated scene layout drifted from SceneStorage");

uint16_t sceneCrcUpdate(uint16_t crc, uint8_t data) {
    crc ^= static_cast<uint16_t>(data) << 8;
    for (uint8_t i = 0; i < 8; ++i) {
        crc = (crc & 0x8000) ? static_cast<uint16_t>((crc << 1) ^ 0x1021)
                             : static_cast<uint16_t>(crc << 1);
    }
    return crc;
}

template <typename T> uint16_t sceneRecordCrc(const T &record) {
    const auto *bytes = reinterpret_cast<const uint8_t *>(&record);
    uint16_t crc = 0xFFFF;
    constexpr size_t payloadOffset = sizeof(record.version) + sizeof(record.crc);
    for (size_t i = payloadOffset; i < sizeof(T); ++i) {
        crc = sceneCrcUpdate(crc, bytes[i]);
    }
    return crc;
}

template <typename T> bool legacySceneRecordValid(const T &record) {
    return record.version == 1 && record.occupied != 0 &&
           record.crc == sceneRecordCrc(record);
}

MIDISlot::EfSettings sanitizeEfSettings(const MIDISlot::EfSettings &settings);
MIDISlot::EfRuntime sanitizeEfRuntime(const MIDISlot::EfRuntime &runtime);

MIDISlot upgradeLegacySlotV6(const LegacyMIDISlotV6 &legacy) {
    MIDISlot upgraded{};
    upgraded.type = legacy.type;
    upgraded.midiChannel = legacy.midiChannel;
    upgraded.data1 = legacy.data1;
    upgraded.active = legacy.active;
    upgraded.arpNote = legacy.arpNote;
    upgraded.sysexLength = legacy.sysexLength;
    upgraded.efSettings = sanitizeEfSettings(legacy.efSettings);
    upgraded.sysexTemplate = legacy.sysexTemplate;
    upgraded.ef = sanitizeEfRuntime(legacy.ef);
    upgraded.setEnvelopeFollowerIndex(upgraded.ef.followerIndex);
    upgraded.arg = sanitizeSlotArg(legacy.arg);
    upgraded.lfo = SlotLfoConfig{};
    return upgraded;
}

SceneStorage::ConfigState upgradeLegacyConfigStateV6(const LegacyConfigStateV6 &legacy) {
    SceneStorage::ConfigState upgraded{};
    upgraded.version = legacy.version;
    upgraded.potChannels = legacy.potChannels;
    upgraded.potCCNumbers = legacy.potCCNumbers;
    for (uint8_t i = 0; i < NUM_SLOTS; ++i) {
        upgraded.slots[i] = upgradeLegacySlotV6(legacy.slots[i]);
    }
    upgraded.argEnabled = legacy.argEnabled;
    upgraded.argMethod = legacy.argMethod;
    upgraded.argSourceA = legacy.argSourceA;
    upgraded.argSourceB = legacy.argSourceB;
    upgraded.envelopeMode = legacy.envelopeMode;
    upgraded.ledBrightness = legacy.ledBrightness;
    upgraded.ledMode = legacy.ledMode;
    upgraded.ledR = legacy.ledR;
    upgraded.ledG = legacy.ledG;
    upgraded.ledB = legacy.ledB;
    upgraded.filterType = legacy.filterType;
    upgraded.filterFrequency = legacy.filterFrequency;
    upgraded.filterQ = legacy.filterQ;
    upgraded.baselines = legacy.baselines;
    return upgraded;
}

bool clearStorageRange(size_t address, size_t length) {
    StorageBackend *storage = ConfigManager::getStorageBackend();
    if (!storage->contains(static_cast<int>(address), length)) return false;
    for (size_t offset = 0; offset < length; ++offset) {
        const int target = static_cast<int>(address + offset);
        if (!storage->update(target, 0x00) || storage->read(target) != 0x00) return false;
    }
    return true;
}

ConfigManager::MigrationResult relocateStorageRangeUp(size_t sourceBegin, size_t sourceEnd,
                                                      size_t shift) {
    StorageBackend *storage = ConfigManager::getStorageBackend();
    if (sourceBegin > sourceEnd || sourceEnd + shift > storage->length()) {
        return ConfigManager::MigrationResult::InsufficientStorage;
    }
    for (size_t source = sourceEnd; source-- > sourceBegin;) {
        const uint8_t value = storage->read(static_cast<int>(source));
        const int destination = static_cast<int>(source + shift);
        if (!storage->update(destination, value)) {
            return ConfigManager::MigrationResult::WriteFailure;
        }
        if (storage->read(destination) != value) {
            return ConfigManager::MigrationResult::VerificationFailure;
        }
    }
    return ConfigManager::MigrationResult::Success;
}

ConfigManager::MigrationResult migrateSchema6SceneStorage() {
    constexpr size_t oldMacroStorage = EEPROM_PROFILE_MODULATION_BASE;
    constexpr size_t oldMacroBytes = sizeof(LegacyMacroRecordV6);
    constexpr size_t oldSceneStorage = oldMacroStorage + oldMacroBytes;

    // Destinations are higher and records grow with MIDISlot, so migrate the
    // six scene records from high to low before converting the macro record.
    for (int slot = static_cast<int>(SceneStorage::kSceneSlotCount) - 1; slot >= 0; --slot) {
        LegacySceneRecordV6 legacy{};
        const size_t source = oldSceneStorage + static_cast<size_t>(slot) *
                                                    sizeof(LegacySceneRecordV6);
        storageGet(static_cast<int>(source), legacy);
        const size_t destination = SceneStorage::kSceneStorageBase +
                                   static_cast<size_t>(slot) * SceneStorage::kSceneRecordBytes;
        if (!legacySceneRecordValid(legacy)) {
            if (!clearStorageRange(destination, SceneStorage::kSceneRecordBytes)) {
                return ConfigManager::MigrationResult::WriteFailure;
            }
            continue;
        }
        MigratedSceneRecord migrated{};
        migrated.version = legacy.version;
        migrated.occupied = legacy.occupied;
        std::memcpy(migrated.name, legacy.name, sizeof(migrated.name));
        migrated.state = upgradeLegacyConfigStateV6(legacy.state);
        migrated.crc = sceneRecordCrc(migrated);
        if (!storagePutVerified(static_cast<int>(destination), migrated)) {
            return ConfigManager::MigrationResult::VerificationFailure;
        }
    }

    LegacyMacroRecordV6 legacyMacro{};
    storageGet(static_cast<int>(oldMacroStorage), legacyMacro);
    if (legacySceneRecordValid(legacyMacro)) {
        MigratedMacroRecord migrated{};
        migrated.version = legacyMacro.version;
        migrated.occupied = legacyMacro.occupied;
        migrated.state = upgradeLegacyConfigStateV6(legacyMacro.state);
        migrated.crc = sceneRecordCrc(migrated);
        if (!storagePutVerified(static_cast<int>(SceneStorage::kMacroStorageAddress), migrated)) {
            return ConfigManager::MigrationResult::VerificationFailure;
        }
    } else {
        if (!clearStorageRange(SceneStorage::kMacroStorageAddress,
                               SceneStorage::kMacroRecordBytes)) {
            return ConfigManager::MigrationResult::WriteFailure;
        }
    }

    // The former macro/scene prefix is now the four empty per-profile
    // modulation-extension blocks.
    if (!clearStorageRange(EEPROM_PROFILE_MODULATION_BASE,
                           EEPROM_PROFILE_MODULATION_BLOCK_SIZE * NUM_PROFILES)) {
        return ConfigManager::MigrationResult::WriteFailure;
    }
    return ConfigManager::MigrationResult::Success;
}

// Reject obviously corrupt legacy filter coefficients before reuse.
bool filterCoefficientsLookSane(float freq, float q) {
    if (!std::isfinite(freq) || !std::isfinite(q))
        return false;
    if (freq < kMinFilterFrequency || freq > kMaxFilterFrequency)
        return false;
    if (q < kMinFilterQ || q > kMaxFilterQ)
        return false;
    return true;
}

// Validate an EF filter enum.
bool filterTypeIsValid(MIDISlot::EfSettings::FilterType type) {
    switch (type) {
    case MIDISlot::EfSettings::FilterType::Linear:
    case MIDISlot::EfSettings::FilterType::OppositeLinear:
    case MIDISlot::EfSettings::FilterType::Exponential:
    case MIDISlot::EfSettings::FilterType::Random:
    case MIDISlot::EfSettings::FilterType::Lowpass:
    case MIDISlot::EfSettings::FilterType::Highpass:
    case MIDISlot::EfSettings::FilterType::Bandpass:
        return true;
    }
    return false;
}

// Sanitize an envelope payload.
SlotEnvelopePayload sanitizeEnvelopePayloadImpl(const SlotEnvelopePayload &payload) {
    SlotEnvelopePayload sanitized = payload;
    if (sanitized.filterType > static_cast<uint8_t>(EnvelopeFollower::BANDPASS)) {
        sanitized.filterType = static_cast<uint8_t>(EnvelopeFollower::LINEAR);
    }
    if (!std::isfinite(sanitized.frequency)) {
        sanitized.frequency = kMinFilterFrequency;
    }
    sanitized.frequency = constrain(sanitized.frequency, kMinFilterFrequency, kMaxFilterFrequency);
    if (!std::isfinite(sanitized.q)) {
        sanitized.q = kMinFilterQ;
    }
    sanitized.q = constrain(sanitized.q, kMinFilterQ, kMaxFilterQ);
    return sanitized;
}

SlotEnvelopePayload persistFilterTailImpl(const SlotEnvelopePayload &payload) {
    SlotEnvelopePayload sanitized = sanitizeEnvelopePayloadImpl(payload);
    storageUpdate(EEPROM_ENVELOPE_TYPES, sanitized.filterType);
    storagePut(EEPROM_FILTER_FREQ, sanitized.frequency);
    storagePut(EEPROM_FILTER_Q, sanitized.q);
    return sanitized;
}

bool persistFilterTailVerified(const SlotEnvelopePayload &payload) {
    const SlotEnvelopePayload sanitized = sanitizeEnvelopePayloadImpl(payload);
    return storageUpdateVerified(EEPROM_ENVELOPE_TYPES, sanitized.filterType) &&
           storagePutVerified(EEPROM_FILTER_FREQ, sanitized.frequency) &&
           storagePutVerified(EEPROM_FILTER_Q, sanitized.q);
}

void maybeRescueFilterTailFromLegacy() {
    float freq = 0.0f;
    float q = 0.0f;
    storageGet(EEPROM_FILTER_FREQ, freq);
    storageGet(EEPROM_FILTER_Q, q);
    if (filterCoefficientsLookSane(freq, q))
        return;

    SlotEnvelopePayload legacy{};
    legacy.filterType = storageRead(EEPROM_ENVELOPE_TYPES);
    storageGet(EEPROM_LEGACY_FILTER_FREQ, legacy.frequency);
    storageGet(EEPROM_LEGACY_FILTER_Q, legacy.q);
    persistFilterTailImpl(legacy);
}

SlotEnvelopePayload settingsToPayload(const MIDISlot::EfSettings &settings) {
    SlotEnvelopePayload payload{};
    payload.filterType = static_cast<uint8_t>(settings.filterType);
    payload.frequency = settings.frequency;
    payload.q = settings.q;
    return payload;
}

void applyPayloadToSettings(const SlotEnvelopePayload &payload, MIDISlot::EfSettings &settings) {
    settings.filterType = static_cast<MIDISlot::EfSettings::FilterType>(
        constrain(payload.filterType, static_cast<uint8_t>(0),
                  static_cast<uint8_t>(MIDISlot::EfSettings::FilterType::Bandpass)));
    settings.frequency = payload.frequency;
    settings.q = payload.q;
}

MIDISlot::EfSettings sanitizeEfSettings(const MIDISlot::EfSettings &settings) {
    MIDISlot::EfSettings sanitized = settings;
    if (!filterTypeIsValid(sanitized.filterType)) {
        sanitized.filterType = MIDISlot::EfSettings::FilterType::Linear;
    }
    SlotEnvelopePayload payload = sanitizeEnvelopePayloadImpl(settingsToPayload(sanitized));
    applyPayloadToSettings(payload, sanitized);
    if (sanitized.oversample == 0)
        sanitized.oversample = 1;
    if (!std::isfinite(sanitized.smoothing))
        sanitized.smoothing = 0.2f;
    sanitized.smoothing = constrain(sanitized.smoothing, 0.0f, 1.0f);
    if (!std::isfinite(sanitized.baseline))
        sanitized.baseline = 0.0f;
    if (!std::isfinite(sanitized.gain))
        sanitized.gain = 1.0f;
    if (sanitized.efMode > static_cast<uint8_t>(EnvelopeFollower::EFMode::Follower)) {
        sanitized.efMode = static_cast<uint8_t>(EnvelopeFollower::EFMode::Peak);
    }
    sanitized.autoBaseline = sanitized.autoBaseline ? 1 : 0;
    sanitized.autoGain = sanitized.autoGain ? 1 : 0;
    sanitized.gateThreshold = constrain(sanitized.gateThreshold, 0, 127);
    sanitized.gateHysteresis = constrain(sanitized.gateHysteresis, 0, 127);
    sanitized.activityThreshold = constrain(sanitized.activityThreshold, 0, 127);
    sanitized.gainTarget = constrain(sanitized.gainTarget, 0, 127);
    sanitized.attackMs = static_cast<uint16_t>(std::max<uint16_t>(1, sanitized.attackMs));
    sanitized.releaseMs = static_cast<uint16_t>(std::max<uint16_t>(1, sanitized.releaseMs));
    sanitized.rmsWindowMs = static_cast<uint16_t>(std::max<uint16_t>(1, sanitized.rmsWindowMs));
    sanitized.baselineTauMs = static_cast<uint16_t>(std::max<uint16_t>(1, sanitized.baselineTauMs));
    sanitized.gainTauMs = static_cast<uint16_t>(std::max<uint16_t>(1, sanitized.gainTauMs));
    return sanitized;
}

MIDISlot::EfRuntime sanitizeEfRuntime(const MIDISlot::EfRuntime &runtime) {
    MIDISlot::EfRuntime sanitized = runtime;
    if (sanitized.followerIndex < -1)
        sanitized.followerIndex = -1;
    return sanitized;
}

} // namespace

// ───────────── ConfigManager member implementations ─────────────

void ConfigManager::loadLegacyARGSettings() {
    legacyArg.mode = storageRead(EEPROM_ARG_MODE);
    legacyArg.method = storageRead(EEPROM_ARG_METHOD);
    legacyArg.enable = storageRead(EEPROM_ARG_ENABLE);

    const uint8_t rawA = storageRead(EEPROM_ARG_ENV_A);
    const uint8_t rawB = storageRead(EEPROM_ARG_ENV_B);

    int idxA = envelopeIndexFromAnalogPin(rawA);
    if (idxA < 0) {
        idxA = (rawA < NUM_ENVELOPES) ? rawA : 0;
    }
    int idxB = envelopeIndexFromAnalogPin(rawB);
    if (idxB < 0) {
        idxB = (rawB < NUM_ENVELOPES) ? rawB : ((idxA + 1) % NUM_ENVELOPES);
    }

    legacyArg.sourceA = static_cast<uint8_t>(idxA % NUM_ENVELOPES);
    legacyArg.sourceB = static_cast<uint8_t>(idxB % NUM_ENVELOPES);
    if (legacyArg.sourceA == legacyArg.sourceB) {
        legacyArg.sourceB = (legacyArg.sourceA + 1) % NUM_ENVELOPES;
    }

    SlotARGConfig defaults{};
    defaults.enabled = legacyArg.enable;
    defaults.method = static_cast<ARGMethod>(legacyArg.method);
    defaults.sourceA = legacyArg.sourceA;
    defaults.sourceB = legacyArg.sourceB;
    defaults = sanitizeSlotArg(defaults);

    legacyArg.enable = defaults.enabled;
    legacyArg.method = static_cast<uint8_t>(defaults.method);
    legacyArg.sourceA = defaults.sourceA;
    legacyArg.sourceB = defaults.sourceB;
}

void ConfigManager::migrateLegacyARGSettings() {
    loadLegacyARGSettings();

    storageUpdate(EEPROM_ARG_MODE, legacyArg.mode);
    storageUpdate(EEPROM_ARG_ENABLE, legacyArg.enable);
    storageUpdate(EEPROM_ARG_METHOD, legacyArg.method);
    setEnvelopePair(legacyArg.sourceA, legacyArg.sourceB);
}

bool ConfigManager::slotLooksSane(const MIDISlot &candidate) {
    if (static_cast<uint8_t>(candidate.type) > static_cast<uint8_t>(MIDIMessageType::SysEx)) {
        return false;
    }
    if (candidate.midiChannel < 1 || candidate.midiChannel > 16) {
        return false;
    }
    if (candidate.sysexLength > SysExTemplate::kMaxLength) {
        return false;
    }
    if (candidate.type != MIDIMessageType::SysEx && candidate.sysexLength != 0) {
        return false;
    }
    if (!filterTypeIsValid(candidate.efSettings.filterType)) {
        return false;
    }
    if (candidate.efSettings.efMode > static_cast<uint8_t>(EnvelopeFollower::EFMode::Follower)) {
        return false;
    }
    SlotEnvelopePayload payload = settingsToPayload(candidate.efSettings);
    if (!std::isfinite(payload.frequency) || !std::isfinite(payload.q)) {
        return false;
    }
    SlotARGConfig sanitized = sanitizeSlotArg(candidate.arg);
    if (sanitized.enabled != (candidate.arg.enabled ? 1 : 0)) {
        return false;
    }
    if (sanitized.sourceA != candidate.arg.sourceA || sanitized.sourceB != candidate.arg.sourceB) {
        return false;
    }
    if (sanitized.method != candidate.arg.method) {
        return false;
    }
    const SlotLfoConfig sanitizedLfo = sanitizeSlotLfoConfig(candidate.lfo);
    for (uint8_t i = 0; i < sanitizedLfo.lfo.size(); ++i) {
        if (sanitizedLfo.lfo[i].flags != candidate.lfo.lfo[i].flags ||
            sanitizedLfo.lfo[i].amount != candidate.lfo.lfo[i].amount) {
            return false;
        }
    }
    return true;
}

void ConfigManager::sanitizeSlotArena() {
    _lastMigrationResult = MigrationResult::Success;
    uint16_t storedVersion = 0;
    storageGet(EEPROM_CONFIG_VERSION, storedVersion);

    auto rewriteInvalidArena = [this](bool writeCurrentVersion) {
        StorageBackend *storage = getStorageBackend();
        const bool transactional = storage->supportsTransactions();
        if (transactional && !storage->beginTransaction()) {
            _lastMigrationResult = MigrationResult::WriteFailure;
            return;
        }
        wipeSlotRegion();
        wipeProfileBlocks();
        if (writeCurrentVersion) {
            storagePut(EEPROM_CONFIG_VERSION, static_cast<uint16_t>(CONFIG_VERSION));
        }
        if (transactional && !storage->commitTransaction()) {
            storage->abortTransaction();
            _lastMigrationResult = MigrationResult::VerificationFailure;
        }
    };

    if (storedVersion == CONFIG_VERSION) {
        migrateLegacyARGSettings();
        maybeRescueFilterTailFromLegacy();
        MIDISlot candidate{};
        storageGet(static_cast<int>(EEPROM_SLOT_BASE), candidate);
        if (!slotLooksSane(candidate)) {
            rewriteInvalidArena(false);
        }
        return;
    }

    if (storedVersion == 0 || storedVersion > CONFIG_VERSION) {
        rewriteInvalidArena(true);
        return;
    }

    StorageBackend *storage = getStorageBackend();
    const bool transactional = storage->supportsTransactions();
    if (transactional && !storage->beginTransaction()) {
        _lastMigrationResult = MigrationResult::WriteFailure;
        return;
    }

    _lastMigrationResult = migrateLegacySlotPayloads(storedVersion);
    if (!transactional) return;
    if (_lastMigrationResult != MigrationResult::Success) {
        storage->abortTransaction();
        return;
    }
    if (!storage->commitTransaction()) {
        storage->abortTransaction();
        _lastMigrationResult = MigrationResult::VerificationFailure;
    }
}

void ConfigManager::wipeSlotRegion() {
    MIDISlot blank{};
    blank.midiChannel = 1;
    SlotEnvelopePayload defaultPayload{};
    defaultPayload.filterType = static_cast<uint8_t>(EnvelopeFollower::LINEAR);
    defaultPayload.frequency = kMinFilterFrequency;
    defaultPayload.q = 1.0f;
    applyPayloadToSettings(defaultPayload, blank.efSettings);
    blank.efSettings = sanitizeEfSettings(blank.efSettings);
    blank.ef = sanitizeEfRuntime(blank.ef);
    blank.arg = sanitizeSlotArg(blank.arg);
    blank.lfo = sanitizeSlotLfoConfig(blank.lfo);
    slots.fill(blank);

    persistFilterTail(defaultPayload);

    for (uint8_t i = 0; i < NUM_SLOTS; ++i) {
        const int address = static_cast<int>(EEPROM_SLOT_BASE + i * SLOT_EEPROM_SIZE);
        storagePut(address, blank);
    }

    legacyArg.enable = 0;
    legacyArg.method = static_cast<uint8_t>(ARGMethod::PLUS);
    legacyArg.sourceA = 0;
    legacyArg.sourceB = 1;
    storageUpdate(EEPROM_ARG_ENABLE, legacyArg.enable);
    storageUpdate(EEPROM_ARG_METHOD, legacyArg.method);
    storageUpdate(EEPROM_ARG_ENV_A, legacyArg.sourceA);
    storageUpdate(EEPROM_ARG_ENV_B, legacyArg.sourceB);
}

FLASHMEM ConfigManager::MigrationResult
ConfigManager::migrateLegacySlotPayloads(uint16_t storedVersion) {
    if (storedVersion == 0x0008) {
        // Schema 9 extends the existing per-profile modulation record without
        // moving any storage regions. Keep schema-8 slots, profiles, legacy-v1
        // modulation blocks, and downstream records in place; the modulation
        // loader upgrades v1 records when they are read.
        MIDISlot first{};
        storageGet(static_cast<int>(EEPROM_SLOT_BASE), first);
        if (!slotLooksSane(first)) return MigrationResult::VerificationFailure;

        const uint16_t currentVersion = CONFIG_VERSION;
        if (!storagePutVerified(EEPROM_CONFIG_VERSION, currentVersion)) {
            return MigrationResult::VerificationFailure;
        }
        slots.fill({});
        return MigrationResult::Success;
    }

    if (storedVersion != kLegacyConfigVersion && storedVersion != 0x0004 &&
        storedVersion != 0x0006 && storedVersion != 0x0007) {
        wipeSlotRegion();
        wipeProfileBlocks();
        return MigrationResult::VerificationFailure;
    }

    loadLegacyARGSettings();

    SlotARGConfig defaults{};
    defaults.enabled = legacyArg.enable;
    defaults.method = static_cast<ARGMethod>(legacyArg.method);
    defaults.sourceA = legacyArg.sourceA;
    defaults.sourceB = legacyArg.sourceB;
    defaults = sanitizeSlotArg(defaults);

    legacyArg.enable = defaults.enabled;
    legacyArg.method = static_cast<uint8_t>(defaults.method);
    legacyArg.sourceA = defaults.sourceA;
    legacyArg.sourceB = defaults.sourceB;

    if (storedVersion == 0x0007) {
        // Schema 8 inserts one fixed-size modulation-extension block per
        // profile between profile settings and macro/scene storage. Shift the
        // complete downstream tail upward, copying high addresses first so
        // the overlapping move is safe.
        constexpr Schema7StorageLayout layout = schema7StorageLayout();
        const size_t oldMacroStorage = layout.oldMacroStorage;
        const size_t newMacroStorage = layout.newMacroStorage;
        const size_t tailShift = layout.tailShift;
        constexpr size_t oldRequiredStorage =
            oldMacroStorage + SceneStorage::kMacroRecordBytes +
            static_cast<size_t>(SceneStorage::kSceneSlotCount) *
                SceneStorage::kSceneRecordBytes;
        if (tailShift == 0) return MigrationResult::VerificationFailure;
        const MigrationResult relocation =
            relocateStorageRangeUp(oldMacroStorage, oldRequiredStorage, tailShift);
        if (relocation != MigrationResult::Success) return relocation;
        if (!clearStorageRange(oldMacroStorage, newMacroStorage - oldMacroStorage)) {
            return MigrationResult::WriteFailure;
        }
    } else if (storedVersion == 0x0006) {
        std::array<LegacyMIDISlotV6, NUM_SLOTS> legacySlots{};
        for (uint8_t i = 0; i < NUM_SLOTS; ++i) {
            const int legacyAddress = static_cast<int>(
                EEPROM_SLOT_BASE + static_cast<size_t>(i) * sizeof(LegacyMIDISlotV6));
            storageGet(legacyAddress, legacySlots[i]);
        }

        // Appending four bytes to every slot shifts the filter tail and every
        // profile/macro/scene region that follows it. Relocate that complete
        // downstream tail from high addresses to low so overlap is safe.
        constexpr Schema6StorageLayout layout =
            schema6StorageLayout(sizeof(LegacyMIDISlotV6), sizeof(LegacyMacroRecordV6),
                                 sizeof(LegacySceneRecordV6), SceneStorage::kSceneSlotCount);
        const size_t oldFilterFrequency =
            EEPROM_SLOT_BASE + sizeof(LegacyMIDISlotV6) * NUM_SLOTS;
        const size_t oldFilterQ = oldFilterFrequency + sizeof(float);
        const size_t oldBrownout = oldFilterQ + sizeof(float);

        float legacyFilterFrequency = legacySlots[0].efSettings.frequency;
        float legacyFilterQ = legacySlots[0].efSettings.q;
        uint16_t legacyBrownoutCount = 0;
        storageGet(static_cast<int>(oldFilterFrequency), legacyFilterFrequency);
        storageGet(static_cast<int>(oldFilterQ), legacyFilterQ);
        storageGet(static_cast<int>(oldBrownout), legacyBrownoutCount);

        const MigrationResult relocation = relocateStorageRangeUp(
            layout.oldConfigTail, layout.oldRequiredStorage, layout.tailShift);
        if (relocation != MigrationResult::Success) return relocation;
        for (int i = static_cast<int>(NUM_SLOTS) - 1; i >= 0; --i) {
            const LegacyMIDISlotV6 &legacy = legacySlots[static_cast<size_t>(i)];
            const MIDISlot upgraded = upgradeLegacySlotV6(legacy);
            const int upgradedAddress = static_cast<int>(
                EEPROM_SLOT_BASE + static_cast<size_t>(i) * SLOT_EEPROM_SIZE);
            if (!storagePutVerified(upgradedAddress, upgraded)) {
                return MigrationResult::VerificationFailure;
            }
        }
        SlotEnvelopePayload migratedFilter{};
        migratedFilter.filterType =
            static_cast<uint8_t>(legacySlots[0].efSettings.filterType);
        migratedFilter.frequency = legacyFilterFrequency;
        migratedFilter.q = legacyFilterQ;
        if (!persistFilterTailVerified(migratedFilter) ||
            !storagePutVerified(EEPROM_BROWNOUT_COUNT, legacyBrownoutCount)) {
            return MigrationResult::VerificationFailure;
        }
        const MigrationResult sceneMigration = migrateSchema6SceneStorage();
        if (sceneMigration != MigrationResult::Success) return sceneMigration;
    } else if (storedVersion == kLegacyConfigVersion) {
        struct LegacyMIDISlotV3 {
            MIDIMessageType type;
            uint8_t midiChannel;
            uint8_t data1;
            uint8_t efIndex;
            bool active;
            uint8_t arpNote;
            uint8_t sysexLength;
            std::array<uint8_t, SysExTemplate::kMaxLength> sysexTemplate;
        };
        static_assert(sizeof(LegacyMIDISlotV3) == 23, "Legacy slot struct size drifted");

        SlotEnvelopePayload legacyPayload{};
        legacyPayload.filterType = storageRead(EEPROM_ENVELOPE_TYPES);
        storageGet(EEPROM_LEGACY_FILTER_FREQ, legacyPayload.frequency);
        storageGet(EEPROM_LEGACY_FILTER_Q, legacyPayload.q);
        SlotEnvelopePayload sanitizedPayload = sanitizeEnvelopePayload(legacyPayload);

        if (!persistFilterTailVerified(sanitizedPayload)) {
            return MigrationResult::VerificationFailure;
        }

        for (int i = static_cast<int>(NUM_SLOTS) - 1; i >= 0; --i) {
            LegacyMIDISlotV3 legacy{};
            const int legacyAddress = static_cast<int>(
                EEPROM_SLOT_BASE + static_cast<size_t>(i) * sizeof(LegacyMIDISlotV3));
            storageGet(legacyAddress, legacy);

            MIDISlot upgraded{};
            upgraded.type = legacy.type;
            upgraded.midiChannel = legacy.midiChannel;
            upgraded.data1 = legacy.data1;
            upgraded.ef.followerIndex = static_cast<int8_t>(legacy.efIndex);
            upgraded.active = legacy.active;
            upgraded.arpNote = legacy.arpNote;
            upgraded.sysexLength = legacy.sysexLength;
            upgraded.sysexTemplate = legacy.sysexTemplate;
            applyPayloadToSettings(sanitizedPayload, upgraded.efSettings);
            upgraded.efSettings = sanitizeEfSettings(upgraded.efSettings);
            upgraded.ef = sanitizeEfRuntime(upgraded.ef);
            upgraded.setEnvelopeFollowerIndex(upgraded.ef.followerIndex);
            upgraded.arg = defaults;

            const int upgradedAddress =
                static_cast<int>(EEPROM_SLOT_BASE + static_cast<size_t>(i) * SLOT_EEPROM_SIZE);
            if (!storagePutVerified(upgradedAddress, upgraded)) {
                return MigrationResult::VerificationFailure;
            }
        }
    } else { // storedVersion == 0x0004
        struct LegacyMIDISlotV4 {
            MIDIMessageType type;
            uint8_t midiChannel;
            uint8_t data1;
            uint8_t efIndex;
            uint8_t active;
            uint8_t arpNote;
            uint8_t sysexLength;
            std::array<uint8_t, SysExTemplate::kMaxLength> sysexTemplate;
            SlotEnvelopePayload efPayload;
        };
        static_assert(sizeof(LegacyMIDISlotV4) == 36, "Legacy v4 slot size drifted");

        std::array<LegacyMIDISlotV4, NUM_SLOTS> legacySlots{};
        for (uint8_t i = 0; i < NUM_SLOTS; ++i) {
            const int legacyAddress = static_cast<int>(
                EEPROM_SLOT_BASE + static_cast<size_t>(i) * sizeof(LegacyMIDISlotV4));
            storageGet(legacyAddress, legacySlots[i]);
        }

        for (int i = static_cast<int>(NUM_SLOTS) - 1; i >= 0; --i) {
            const LegacyMIDISlotV4 &legacy = legacySlots[static_cast<size_t>(i)];

            MIDISlot upgraded{};
            upgraded.type = legacy.type;
            upgraded.midiChannel = legacy.midiChannel;
            upgraded.data1 = legacy.data1;
            upgraded.ef.followerIndex = static_cast<int8_t>(legacy.efIndex);
            upgraded.active = legacy.active != 0;
            upgraded.arpNote = legacy.arpNote;
            upgraded.sysexLength = legacy.sysexLength;
            upgraded.sysexTemplate = legacy.sysexTemplate;
            SlotEnvelopePayload migratedPayload = sanitizeEnvelopePayload(legacy.efPayload);
            applyPayloadToSettings(migratedPayload, upgraded.efSettings);
            upgraded.efSettings = sanitizeEfSettings(upgraded.efSettings);
            upgraded.ef = sanitizeEfRuntime(upgraded.ef);
            upgraded.setEnvelopeFollowerIndex(upgraded.ef.followerIndex);
            upgraded.arg = defaults;

            const int upgradedAddress =
                static_cast<int>(EEPROM_SLOT_BASE + static_cast<size_t>(i) * SLOT_EEPROM_SIZE);
            if (!storagePutVerified(upgradedAddress, upgraded)) {
                return MigrationResult::VerificationFailure;
            }
        }

        MIDISlot first{};
        loadSlot(0, first);
        if (!persistFilterTailVerified(settingsToPayload(first.efSettings))) {
            return MigrationResult::VerificationFailure;
        }
    }

    if (!storageUpdateVerified(EEPROM_ARG_ENABLE, legacyArg.enable) ||
        !storageUpdateVerified(EEPROM_ARG_METHOD, legacyArg.method) ||
        !storageUpdateVerified(EEPROM_ARG_ENV_A, legacyArg.sourceA) ||
        !storageUpdateVerified(EEPROM_ARG_ENV_B, legacyArg.sourceB)) {
        return MigrationResult::WriteFailure;
    }
    const uint16_t currentVersion = CONFIG_VERSION;
    if (!storagePutVerified(EEPROM_CONFIG_VERSION, currentVersion)) {
        return MigrationResult::VerificationFailure;
    }

    slots.fill({});
    return MigrationResult::Success;
}

SlotEnvelopePayload ConfigManager::seedSlotEnvelopePayloads(uint8_t filterType, float freq,
                                                            float q) {
    SlotEnvelopePayload payload{};
    payload.filterType = filterType;
    payload.frequency = freq;
    payload.q = q;
    SlotEnvelopePayload sanitized = persistFilterTail(payload);
    for (uint8_t i = 0; i < NUM_SLOTS; ++i) {
        MIDISlot slot{};
        loadSlot(i, slot);
        applyPayloadToSettings(sanitized, slot.efSettings);
        slot.efSettings = sanitizeEfSettings(slot.efSettings);
        slot.ef = sanitizeEfRuntime(slot.ef);
        slot.setEnvelopeFollowerIndex(slot.ef.followerIndex);
        slots[i] = slot;
        saveSlot(i, slots[i]);
    }
    return sanitized;
}

FLASHMEM void ConfigManager::wipeProfileBlocks() {
    // Program-flash LittleFS turns each tiny write into filesystem work. Clear
    // migration regions in block-sized chunks so first boot after an invalid or
    // legacy payload remains bounded instead of issuing thousands of 1-byte
    // flash updates.
    constexpr size_t kClearChunkSize = 256;
    const std::array<uint8_t, kClearChunkSize> zeros{};
    auto clearBlock = [&zeros](uint16_t base, uint16_t size) {
        StorageBackend *storage = ConfigManager::getStorageBackend();
        uint16_t offset = 0;
        while (offset < size) {
            const size_t count =
                std::min<size_t>(zeros.size(), static_cast<size_t>(size - offset));
            if (!storage->writeBytes(static_cast<int>(base + offset), zeros.data(), count)) {
                return false;
            }
            offset = static_cast<uint16_t>(offset + count);
        }
        return true;
    };
    for (uint8_t id = 1; id < NUM_PROFILES; ++id) {
        if (!clearBlock(EEPROM_PROFILE_START(id), EEPROM_PROFILE_BLOCK_SIZE)) return;
    }
    for (uint8_t id = 0; id < NUM_PROFILES; ++id) {
        if (!clearBlock(EEPROM_PROFILE_SETTINGS_START(id), EEPROM_PROFILE_SETTINGS_BLOCK_SIZE))
            return;
    }
    for (uint8_t id = 0; id < NUM_PROFILES; ++id) {
        if (!clearBlock(EEPROM_PROFILE_MODULATION_START(id),
                        EEPROM_PROFILE_MODULATION_BLOCK_SIZE))
            return;
    }
}
