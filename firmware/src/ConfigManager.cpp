// ConfigManager is the repo's long-term memory. It shows how we marshal structs
// into EEPROM, guard against corruption, and fan those bytes back into runtime
// objects without leaking pointers or faith. Treat this file as a crash course
// in embedded persistence: every helper explains why the bounds checks exist
// and how legacy migrations keep older builds from bricking.

#include "ConfigManager.h"
#include "ProfileStorage.h"
#include "EnvelopeFollower.h"
#include "ARGMixer.h" // reuse the shared sanitizeSlotArg implementation
#include "Arpeggiator.h"
#include "LFO/LFOManager.h"
#include "storage/EepromStorageBackend.h"
#include "storage/LittleFsStorageBackend.h"
#include <cmath>
#include <cstddef>
#include <vector>
#include "Log.h"

extern std::vector<EnvelopeFollower> envelopeFollowers;
extern ConfigManager configManager;

// Weak hook lets test firmware skip the heavyweight EF voice refresh logic.
#if defined(__GNUC__) || defined(__clang__)
extern void refreshEfVoicesFromConfig() __attribute__((weak));
#else
extern void refreshEfVoicesFromConfig();
#endif

// Update one slot's EF settings, persist them, and notify any runtime cache
// that mirrors follower assignments.
void saveSlotEfSettings(uint8_t slotIndex, const MIDISlot::EfSettings &settings) {
    if (slotIndex >= NUM_SLOTS) {
        return;
    }

    MIDISlot &slot = configManager.getSlot(slotIndex);
    slot.efSettings = settings;
    configManager.saveSlot(slotIndex, slot);

#if defined(__GNUC__) || defined(__clang__)
    if (refreshEfVoicesFromConfig != nullptr) {
        refreshEfVoicesFromConfig();
    }
#else
    refreshEfVoicesFromConfig();
#endif
}

namespace {
StorageBackend *gStorageBackendOverride = nullptr;

StorageBackend &activeStorageBackend() {
    if (gStorageBackendOverride) {
        return *gStorageBackendOverride;
    }
#if defined(CONFIG_STORAGE_LITTLEFS)
    static LittleFsStorageBackend littleFsBackend;
    static EepromStorageBackend eepromFallbackBackend;
    if (littleFsBackend.ready()) {
        return littleFsBackend;
    }
    return eepromFallbackBackend;
#else
    static EepromStorageBackend defaultBackend;
    return defaultBackend;
#endif
}

uint8_t storageRead(int address) { return activeStorageBackend().read(address); }

void storageUpdate(int address, uint8_t value) { activeStorageBackend().update(address, value); }

template <typename T> void storageGet(int address, T &value) {
    activeStorageBackend().readBytes(address, &value, sizeof(T));
}

template <typename T> void storagePut(int address, const T &value) {
    activeStorageBackend().writeBytes(address, &value, sizeof(T));
}

constexpr uint16_t kLegacyConfigVersion = 0x0003;
constexpr uint16_t kProfileSettingsVersion = PROFILE_SETTINGS_VERSION;
constexpr float kMinFilterFrequency = EF_FILTER_FREQ_MIN_HZ;
constexpr float kMaxFilterFrequency = EF_FILTER_FREQ_MAX_HZ;
constexpr float kMinFilterQ = EF_FILTER_Q_MIN;
constexpr float kMaxFilterQ = EF_FILTER_Q_MAX;
constexpr int kUnassignedEnvelope = -1;

// Computes CRC-16 with the Modbus-flavored 0xA001 polynomial to keep our
// saved configuration blocks honest. Peek at docs/EEPROMLayout.md to see
// where the checksum bunkers down.
static uint16_t crc16_update(uint16_t crc, uint8_t data) {
    crc ^= data;
    for (uint8_t i = 0; i < 8; ++i) {
        if (crc & 1) {
            crc = (crc >> 1) ^ 0xA001;
        } else {
            crc >>= 1;
        }
    }
    return crc;
}

// filterCoefficientsLookSane and maybeRescueFilterTailFromLegacy live in
// SchemaMigration.cpp — they are only needed during boot migration.

// sanitizeProfileEfSettings, sanitizeProfileData, and computeProfileCrc
// are now in ProfileStorage.cpp / ProfileStorage.h.

// Clamp the filter payload that gets mirrored into slot storage and the legacy
// tail region.
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

// Persist the shared "tail" filter payload used by older layouts and recovery
// helpers, returning the sanitized version that was actually written.
SlotEnvelopePayload persistFilterTailImpl(const SlotEnvelopePayload &payload) {
    SlotEnvelopePayload sanitized = sanitizeEnvelopePayloadImpl(payload);
    storageUpdate(EEPROM_ENVELOPE_TYPES, sanitized.filterType);
    storagePut(EEPROM_FILTER_FREQ, sanitized.frequency);
    storagePut(EEPROM_FILTER_Q, sanitized.q);
    return sanitized;
}

// Validate whether an EF filter enum is one of the persisted values we know how
// to map back into runtime behavior.
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

// Extract just the persistable filter payload from a full EF settings block.
SlotEnvelopePayload settingsToPayload(const MIDISlot::EfSettings &settings) {
    SlotEnvelopePayload payload{};
    payload.filterType = static_cast<uint8_t>(settings.filterType);
    payload.frequency = settings.frequency;
    payload.q = settings.q;
    return payload;
}

// Push a persistable filter payload back into a full EF settings block.
void applyPayloadToSettings(const SlotEnvelopePayload &payload, MIDISlot::EfSettings &settings) {
    settings.filterType = static_cast<MIDISlot::EfSettings::FilterType>(
        constrain(payload.filterType, static_cast<uint8_t>(0),
                  static_cast<uint8_t>(MIDISlot::EfSettings::FilterType::Bandpass)));
    settings.frequency = payload.frequency;
    settings.q = payload.q;
}

// Clamp a full EF settings block so corrupt EEPROM or imported JSON cannot
// destabilize follower runtime math.
MIDISlot::EfSettings sanitizeEfSettings(const MIDISlot::EfSettings &settings) {
    // Clamp EF settings so corrupt EEPROM data doesn't destabilize runtime.
    MIDISlot::EfSettings sanitized = settings;
    if (!filterTypeIsValid(sanitized.filterType)) {
        sanitized.filterType = MIDISlot::EfSettings::FilterType::Linear;
    }
    SlotEnvelopePayload payload = sanitizeEnvelopePayloadImpl(settingsToPayload(sanitized));
    applyPayloadToSettings(payload, sanitized);
    sanitized.oversample = static_cast<uint8_t>(constrain(static_cast<int>(sanitized.oversample),
                                                          static_cast<int>(EF_OVERSAMPLE_MIN),
                                                          static_cast<int>(EF_OVERSAMPLE_MAX)));
    if (!std::isfinite(sanitized.smoothing)) {
        sanitized.smoothing = 0.2f;
    }
    sanitized.smoothing = constrain(sanitized.smoothing, 0.0f, 1.0f);
    if (!std::isfinite(sanitized.baseline)) {
        sanitized.baseline = 0.0f;
    }
    if (!std::isfinite(sanitized.gain)) {
        sanitized.gain = 1.0f;
    }
    // Validate EF mode and auto-cal flags.
    if (sanitized.efMode > static_cast<uint8_t>(EnvelopeFollower::EFMode::Follower)) {
        sanitized.efMode = static_cast<uint8_t>(EnvelopeFollower::EFMode::Peak);
    }
    sanitized.autoBaseline = sanitized.autoBaseline ? 1 : 0;
    sanitized.autoGain = sanitized.autoGain ? 1 : 0;
    sanitized.gateThreshold = constrain(sanitized.gateThreshold, 0, 127);
    sanitized.gateHysteresis = constrain(sanitized.gateHysteresis, 0, 127);
    sanitized.activityThreshold = constrain(sanitized.activityThreshold, 0, 127);
    sanitized.gainTarget = constrain(sanitized.gainTarget, 0, 127);
    sanitized.attackMs = static_cast<uint16_t>(constrain(static_cast<int>(sanitized.attackMs),
                                                         static_cast<int>(EF_TIME_MIN_MS),
                                                         static_cast<int>(EF_TIME_MAX_MS)));
    sanitized.releaseMs = static_cast<uint16_t>(constrain(static_cast<int>(sanitized.releaseMs),
                                                          static_cast<int>(EF_TIME_MIN_MS),
                                                          static_cast<int>(EF_TIME_MAX_MS)));
    sanitized.rmsWindowMs = static_cast<uint16_t>(constrain(static_cast<int>(sanitized.rmsWindowMs),
                                                            static_cast<int>(EF_TIME_MIN_MS),
                                                            static_cast<int>(EF_TIME_MAX_MS)));
    sanitized.baselineTauMs = static_cast<uint16_t>(
        constrain(static_cast<int>(sanitized.baselineTauMs), static_cast<int>(EF_TIME_MIN_MS),
                  static_cast<int>(EF_TIME_MAX_MS)));
    sanitized.gainTauMs = static_cast<uint16_t>(constrain(static_cast<int>(sanitized.gainTauMs),
                                                          static_cast<int>(EF_TIME_MIN_MS),
                                                          static_cast<int>(EF_TIME_MAX_MS)));
    return sanitized;
}

// Runtime-only EF fields get a lighter sanitization pass because they are not
// supposed to contain the full persisted tuning set.
MIDISlot::EfRuntime sanitizeEfRuntime(const MIDISlot::EfRuntime &runtime) {
    MIDISlot::EfRuntime sanitized = runtime;
    if (sanitized.followerIndex < -1) {
        sanitized.followerIndex = -1;
    }
    return sanitized;
}
} // namespace

void ConfigManager::setStorageBackend(StorageBackend *backend) {
    gStorageBackendOverride = backend;
}

StorageBackend *ConfigManager::getStorageBackend() { return &activeStorageBackend(); }

// Constructor
ConfigManager::ConfigManager(uint8_t numPots, uint8_t numButtons)
    : _numPots(numPots), _numButtons(numButtons) {}

// Centralized EEPROM health check
bool ConfigManager::checkEEPROMHealth(bool backup, uint16_t base) {
    int address = base + (backup ? EEPROM_MAGIC_ADDRESS + 2 : EEPROM_MAGIC_ADDRESS);
    uint16_t magic = storageRead(address) << 8 | storageRead(address + 1);
    return (magic == (backup ? EEPROM_MAGIC_BACKUP : EEPROM_MAGIC_PRIMARY));
}

// Write magic number to EEPROM
void ConfigManager::writeMagicNumber(bool backup, uint16_t base) {
    int address = base + (backup ? EEPROM_MAGIC_ADDRESS + 2 : EEPROM_MAGIC_ADDRESS);
    uint16_t magic = backup ? EEPROM_MAGIC_BACKUP : EEPROM_MAGIC_PRIMARY;
    storageUpdate(address, (magic >> 8) & 0xFF);
    storageUpdate(address + 1, magic & 0xFF);
}

// Save configuration with verification and backup
void ConfigManager::saveConfiguration() {
    // Write primary first, then read-verify; backup is refreshed only when verification fails.
    uint16_t base = EEPROM_PROFILE_START(0);
    writeEEPROM(false, base); // Write primary
    writeMagicNumber(false, base);

    // Verify
    std::vector<uint8_t> temp;
    if (!loadConfiguration(temp, base)) {
        LOG_PRINTLN(
            "{\"type\":\"info\",\"message\":\"Primary EEPROM write failed, saving to backup.\"}");
        writeEEPROM(true, base);
        writeMagicNumber(true, base);
    }
}

// Load configuration (primary)
bool ConfigManager::loadConfiguration(std::vector<uint8_t> &potChannels, uint16_t base) {
    // Recovery order: primary -> backup -> defaults.
    if (checkEEPROMHealth(false, base)) {
        readEEPROM(false, base);
        bool needsRewrite = false;
        if (_stored.version != CONFIG_VERSION) {
            if (_stored.version == kLegacyConfigVersion) {
                needsRewrite = true;
            } else {
                LOG_PRINTLN("{\"type\":\"error\",\"message\":\"Config version mismatch.\"}");
                resetConfiguration(potChannels, true);
                return false;
            }
        }
        bool includeProfile = (base == EEPROM_PROFILE_START(0));
        if (_stored.crc != calculateCRC(includeProfile)) {
            LOG_PRINTLN("{\"type\":\"error\",\"message\":\"Config CRC mismatch.\"}");
            resetConfiguration(potChannels, true);
            return false;
        }
        if (_stored.activeProfile >= NUM_PROFILES) {
            _stored.activeProfile = 0;
            needsRewrite = true;
        }
        potChannels.clear();
        for (uint8_t i = 0; i < _numPots; i++) {
            potChannels.push_back(_stored.potChannels[i]);
        }
        if (needsRewrite) {
            _stored.version = CONFIG_VERSION;
            saveConfiguration();
        }
        return true;
    }
    LOG_PRINTLN("{\"type\":\"error\",\"message\":\"Primary EEPROM corrupted, trying backup.\"}");
    return loadBackupConfiguration(potChannels, base);
}

// Load configuration (backup)
bool ConfigManager::loadBackupConfiguration(std::vector<uint8_t> &potChannels, uint16_t base) {
    // Backup path mirrors primary validation and emits a recovery event for UI messaging.
    if (checkEEPROMHealth(true, base)) {
        readEEPROM(true, base);
        bool needsRewrite = false;
        if (_stored.version != CONFIG_VERSION) {
            if (_stored.version == kLegacyConfigVersion) {
                needsRewrite = true;
            } else {
                LOG_PRINTLN("{\"type\":\"error\",\"message\":\"Backup config version mismatch.\"}");
                resetConfiguration(potChannels, true);
                return false;
            }
        }
        bool includeProfile = (base == EEPROM_PROFILE_START(0));
        if (_stored.crc != calculateCRC(includeProfile)) {
            LOG_PRINTLN("{\"type\":\"error\",\"message\":\"Backup CRC mismatch.\"}");
            resetConfiguration(potChannels, true);
            return false;
        }
        if (_stored.activeProfile >= NUM_PROFILES) {
            _stored.activeProfile = 0;
            needsRewrite = true;
        }
        potChannels.clear();
        for (uint8_t i = 0; i < _numPots; i++) {
            potChannels.push_back(_stored.potChannels[i]);
        }
        if (needsRewrite) {
            _stored.version = CONFIG_VERSION;
            saveConfiguration();
        }
        _lastRecoveryEvent = RecoveryEvent::kBackupRestored;
        return true;
    }
    LOG_PRINTLN(
        "{\"type\":\"error\",\"message\":\"Backup EEPROM corrupted, resetting to defaults.\"}");
    resetConfiguration(potChannels, true);
    return false;
}

// Internal read from EEPROM
void ConfigManager::readEEPROM(bool backup, uint16_t base) {
    int offset = base + (backup ? EEPROM_BACKUP_START : EEPROM_START_ADDRESS);
    for (uint8_t i = 0; i < _numPots; i++) {
        _stored.potChannels[i] = storageRead(offset + EEPROM_POT_CHANNELS + i);
        _stored.potCCNumbers[i] = storageRead(offset + EEPROM_POT_CC + i);
    }
    if (base == EEPROM_PROFILE_START(0)) {
        _stored.activeProfile = storageRead(offset + EEPROM_ACTIVE_PROFILE);
        _stored.ledMode = storageRead(offset + EEPROM_LED_MODE);
        uint8_t idleFloor = storageRead(offset + EEPROM_EF_IDLE_FLOOR);
        uint8_t idleFloorCheck = storageRead(offset + EEPROM_EF_IDLE_FLOOR_CHECK);
        g_efIdleFloor = ((idleFloor ^ 0xFF) == idleFloorCheck && idleFloor <= 127)
                            ? idleFloor
                            : EF_IDLE_FLOOR_DEFAULT;
    }
    storageGet(offset + EEPROM_CONFIG_VERSION, _stored.version);
    storageGet(offset + EEPROM_CONFIG_CRC, _stored.crc);
}

// Internal write to EEPROM
void ConfigManager::writeEEPROM(bool backup, uint16_t base) {
    int offset = base + (backup ? EEPROM_BACKUP_START : EEPROM_START_ADDRESS);
    uint16_t crc = calculateCRC(base == EEPROM_PROFILE_START(0));
    for (uint8_t i = 0; i < _numPots; i++) {
        storageUpdate(offset + EEPROM_POT_CHANNELS + i, _stored.potChannels[i]);
        storageUpdate(offset + EEPROM_POT_CC + i, _stored.potCCNumbers[i]);
    }
    if (base == EEPROM_PROFILE_START(0)) {
        storageUpdate(offset + EEPROM_ACTIVE_PROFILE, _stored.activeProfile);
        storageUpdate(offset + EEPROM_LED_MODE, _stored.ledMode);
        storageUpdate(offset + EEPROM_EF_IDLE_FLOOR, g_efIdleFloor);
        storageUpdate(offset + EEPROM_EF_IDLE_FLOOR_CHECK, g_efIdleFloor ^ 0xFF);
    }
    storagePut(offset + EEPROM_CONFIG_VERSION, (uint16_t)CONFIG_VERSION);
    storagePut(offset + EEPROM_CONFIG_CRC, crc);
    _stored.version = CONFIG_VERSION;
    _stored.crc = crc;
}

uint16_t ConfigManager::calculateCRC(bool includeProfile) const {
    uint16_t crc = 0xFFFF;
    for (uint8_t i = 0; i < _numPots; ++i) {
        crc = crc16_update(crc, _stored.potChannels[i]);
    }
    for (uint8_t i = 0; i < _numPots; ++i) {
        crc = crc16_update(crc, _stored.potCCNumbers[i]);
    }
    if (includeProfile) {
        crc = crc16_update(crc, _stored.activeProfile);
        crc = crc16_update(crc, _stored.ledMode);
    }
    return crc;
}

// Load a profile block into the current working config
void ConfigManager::loadProfile(uint8_t id) {
    // Clamp to valid profile slots so EEPROM reads stay in range.
    if (id >= NUM_PROFILES) {
        id = 0;
    }
    uint16_t base = EEPROM_PROFILE_START(id);
    if (checkEEPROMHealth(false, base)) {
        readEEPROM(false, base);
        if (_stored.version != CONFIG_VERSION || _stored.crc != calculateCRC(false)) {
            LOG_PRINTLN(
                "{\"type\":\"error\",\"message\":\"Profile slot corrupted, using defaults.\"}");
        }
    } else if (checkEEPROMHealth(true, base)) {
        readEEPROM(true, base);
        if (_stored.version != CONFIG_VERSION || _stored.crc != calculateCRC(false)) {
            LOG_PRINTLN(
                "{\"type\":\"error\",\"message\":\"Profile slot corrupted, using defaults.\"}");
        }
    } else {
        LOG_PRINTLN("{\"type\":\"error\",\"message\":\"Profile slot corrupted, using defaults.\"}");
    }
}

// Save the current config into the given profile block
void ConfigManager::saveProfile(uint8_t id) {
    // Clamp to valid profile slots so EEPROM writes stay in range.
    if (id >= NUM_PROFILES) {
        id = 0;
    }
    uint16_t base = EEPROM_PROFILE_START(id);
    // Capture the new slot map in the backup region first so an interrupted write
    // still leaves a complete copy to recover from.
    writeEEPROM(true, base);
    writeMagicNumber(true, base);
    writeEEPROM(false, base);
    writeMagicNumber(false, base);
    std::vector<uint8_t> temp;
    if (!loadConfiguration(temp, base)) {
        LOG_PRINTLN("{\"type\":\"info\",\"message\":\"Primary EEPROM corrupted after save; backup "
                    "contains latest profile.\"}");
    }
}

bool ConfigManager::loadProfileSettings(uint8_t id, ProfileData &profile) const {
    // Pull the extended profile payload from EEPROM and validate checksum.
    if (id >= NUM_PROFILES) {
        return false;
    }
    const uint16_t base = EEPROM_PROFILE_SETTINGS_START(id);
    ProfileData stored{};
    storageGet(base, stored);
    if (stored.version != PROFILE_SETTINGS_VERSION) {
        return false;
    }
    uint16_t crc = computeProfileCrc(stored);
    if (crc != stored.crc) {
        return false;
    }
    // Sanitize on load so bad data never reaches runtime.
    profile = sanitizeProfileData(stored);
    profile.crc = computeProfileCrc(profile);
    return true;
}

bool ConfigManager::saveProfileSettings(uint8_t id, const ProfileData &profile) {
    // Save a sanitized profile payload with a fresh CRC.
    if (id >= NUM_PROFILES) {
        return false;
    }
    ProfileData sanitized = sanitizeProfileData(profile);
    sanitized.crc = computeProfileCrc(sanitized);
    const uint16_t base = EEPROM_PROFILE_SETTINGS_START(id);
    storagePut(base, sanitized);
    return true;
}

void ConfigManager::setActiveProfile(uint8_t id) {
    // Persist the profile index so power cycles restore the last slot.
    if (id >= NUM_PROFILES) {
        id = 0;
    }
    _stored.activeProfile = id;
    saveConfiguration();
}

// Initialize configuration
void ConfigManager::begin(std::vector<uint8_t> &potChannels) {
    sanitizeSlotArena();
    // 1) Load every MIDISlot from EEPROM into our in-RAM array
    for (uint8_t i = 0; i < NUM_SLOTS; ++i) {
        loadSlot(i, slots[i]);
    }
    // 2) Pull out the existing pot → MIDI channel assignments
    //    (assuming _stored.potChannels was filled by readEEPROM)
    potChannels.clear();
    for (uint8_t i = 0; i < _numPots; ++i) {
        potChannels.push_back(_stored.potChannels[i]);
    }
}

void ConfigManager::loadSlot(uint8_t idx, MIDISlot &dest) {
    MIDISlot temp{};
    const int address = static_cast<int>(EEPROM_SLOT_BASE + idx * SLOT_EEPROM_SIZE);
    storageGet(address, temp);
    if (temp.sysexLength > SysExTemplate::kMaxLength) {
        temp.sysexLength = 0;
        temp.sysexTemplate.fill(0);
    }
    temp.efSettings = sanitizeEfSettings(temp.efSettings);
    temp.ef = sanitizeEfRuntime(temp.ef);
    temp.setEnvelopeFollowerIndex(temp.ef.followerIndex);
    temp.arg = sanitizeSlotArg(temp.arg);
    dest = temp;
    if (dest.arpNote > 127)
        dest.arpNote = dest.data1;
}

void ConfigManager::saveSlot(uint8_t idx, const MIDISlot &src) {
    MIDISlot sanitized = src;
    if (sanitized.sysexLength > SysExTemplate::kMaxLength) {
        sanitized.sysexLength = SysExTemplate::kMaxLength;
    }
    for (uint8_t i = sanitized.sysexLength; i < SysExTemplate::kMaxLength; ++i) {
        sanitized.sysexTemplate[i] = 0;
    }
    sanitized.efSettings = sanitizeEfSettings(sanitized.efSettings);
    sanitized.ef = sanitizeEfRuntime(sanitized.ef);
    sanitized.setEnvelopeFollowerIndex(sanitized.ef.followerIndex);
    sanitized.arg = sanitizeSlotArg(sanitized.arg);
    const int address = static_cast<int>(EEPROM_SLOT_BASE + idx * SLOT_EEPROM_SIZE);
    storagePut(address, sanitized);
    slots[idx] = sanitized;
}

// Potentiometer accessors
uint8_t ConfigManager::getPotChannel(uint8_t potIndex) const {
    return _stored.potChannels.at(potIndex);
}

uint8_t ConfigManager::getPotCCNumber(uint8_t potIndex) const {
    return _stored.potCCNumbers.at(potIndex);
}

void ConfigManager::setPotChannel(uint8_t potIndex, uint8_t channel) {
    if (potIndex < _numPots) {
        _stored.potChannels[potIndex] = channel;
        if (potIndex < slots.size()) {
            MIDISlot &slot = slots[potIndex];
            if (slot.midiChannel != channel) {
                slot.midiChannel = channel;
                saveSlot(potIndex, slot);
            }
        }
    }
}

void ConfigManager::setPotCCNumber(uint8_t potIndex, uint8_t ccNumber) {
    if (potIndex < _numPots) {
        _stored.potCCNumbers[potIndex] = ccNumber;
        if (potIndex < slots.size()) {
            MIDISlot &slot = slots[potIndex];
            if (slot.data1 != ccNumber) {
                slot.data1 = ccNumber;
                saveSlot(potIndex, slot);
            }
        }
    }
}

// Envelope settings
bool ConfigManager::loadEnvelopeSettings(std::map<int, MIDISlot::EfSettings> &potToEnvelopeMap,
                                         std::vector<EnvelopeFollower> &envelopes) {
    bool allFound = true;

    potToEnvelopeMap.clear();
    for (uint8_t potIndex = 0; potIndex < NUM_POTS; ++potIndex) {
        int storedValue = storageRead(EEPROM_ENVELOPE_ASSIGNMENTS + potIndex);
        int envelopeIndex = (storedValue == 0xFF) ? kUnassignedEnvelope : storedValue;
        MIDISlot::EfSettings settings = {};
        if (potIndex < slots.size()) {
            settings = slots[potIndex].efSettings;
            settings.followerIndex = slots[potIndex].ef.followerIndex;
        }
        if (envelopeIndex >= 0 && envelopeIndex < static_cast<int>(envelopes.size())) {
            settings.followerIndex = static_cast<int8_t>(envelopeIndex);
            settings = sanitizeEfSettings(settings);
            potToEnvelopeMap.emplace(potIndex, settings);
            if (potIndex < slots.size()) {
                slots[potIndex].efSettings = settings;
                slots[potIndex].setEnvelopeFollowerIndex(settings.followerIndex);
            }
        } else if (potIndex < slots.size()) {
            slots[potIndex].setEnvelopeFollowerIndex(-1);
        }
    }

    for (size_t envIndex = 0; envIndex < envelopes.size(); ++envIndex) {
        float baseline;
        storageGet(EEPROM_EF_BASELINES + envIndex * sizeof(float), baseline);

        envelopes[envIndex].setVref(g_vref); // always refresh Vref
        if (!std::isnan(baseline)) {
            envelopes[envIndex].setBaseline(baseline);
            envelopeConfig.baselines[envIndex] = baseline;
        } else {
            envelopeConfig.baselines[envIndex] = 0.0f;
            allFound = false;
        }
    }

    for (auto &entry : potToEnvelopeMap) {
        auto &settings = entry.second;
        const int follower = settings.followerIndex;
        if (follower < 0 || follower >= static_cast<int>(envelopes.size())) {
            continue;
        }
        settings.baseline = envelopes[follower].getBaseline();
        settings = sanitizeEfSettings(settings);
        envelopes[follower].configureFromEfSettings(settings);
        if (static_cast<size_t>(entry.first) < slots.size()) {
            slots[entry.first].efSettings = settings;
            slots[entry.first].setEnvelopeFollowerIndex(settings.followerIndex);
        }
    }
    return allFound;
}

void ConfigManager::saveEnvelopeSettings(
    const std::map<int, MIDISlot::EfSettings> &potToEnvelopeMap,
    const std::vector<EnvelopeFollower> &envelopes) {
    constexpr uint8_t kUnassignedMarker = 0xFF;
    for (uint8_t potIndex = 0; potIndex < NUM_POTS; ++potIndex) {
        auto it = potToEnvelopeMap.find(potIndex);
        int envelopeIndex = kUnassignedEnvelope;
        if (it != potToEnvelopeMap.end()) {
            envelopeIndex = it->second.followerIndex;
        }
        uint8_t storedValue =
            (envelopeIndex >= 0 && envelopeIndex < static_cast<int>(envelopes.size()))
                ? static_cast<uint8_t>(envelopeIndex)
                : kUnassignedMarker;
        storageUpdate(EEPROM_ENVELOPE_ASSIGNMENTS + potIndex, storedValue);

        if (potIndex < slots.size()) {
            MIDISlot &slot = slots[potIndex];
            if (it != potToEnvelopeMap.end()) {
                MIDISlot::EfSettings sanitized = sanitizeEfSettings(it->second);
                slot.efSettings = sanitized;
                if (envelopeIndex >= 0 && envelopeIndex < static_cast<int>(envelopes.size())) {
                    slot.setEnvelopeFollowerIndex(static_cast<int8_t>(envelopeIndex));
                    slot.efSettings.baseline = envelopes[envelopeIndex].getBaseline();
                } else {
                    slot.setEnvelopeFollowerIndex(-1);
                }
            } else {
                slot.setEnvelopeFollowerIndex(-1);
            }
        }
    }
    for (size_t i = 0; i < envelopes.size(); ++i) {
        envelopeConfig.baselines[i] = envelopes[i].getBaseline();
        storagePut(EEPROM_EF_BASELINES + i * sizeof(float), envelopeConfig.baselines[i]);
    }
}

void ConfigManager::saveEnvelopeBaseline(uint8_t envIndex, float baseline) {
    if (envIndex < NUM_ENVELOPES) {
        envelopeConfig.baselines[envIndex] = baseline;
        storagePut(EEPROM_EF_BASELINES + envIndex * sizeof(float), baseline);
    }
}

// LED settings
void ConfigManager::loadLEDSettings(uint8_t &brightness, CRGB &color) {
    brightness = storageRead(EEPROM_LED_BRIGHTNESS);
    color.r = storageRead(EEPROM_LED_COLOR);
    color.g = storageRead(EEPROM_LED_COLOR + 1);
    color.b = storageRead(EEPROM_LED_COLOR + 2);
}

void ConfigManager::saveLEDSettings(uint8_t brightness, CRGB color) {
    storageUpdate(EEPROM_LED_BRIGHTNESS, brightness);
    storageUpdate(EEPROM_LED_COLOR, color.r);
    storageUpdate(EEPROM_LED_COLOR + 1, color.g);
    storageUpdate(EEPROM_LED_COLOR + 2, color.b);
}

void ConfigManager::setLedMode(LedMode mode) {
    _stored.ledMode = static_cast<uint8_t>(sanitizeLedMode(mode));
    storageUpdate(EEPROM_LED_MODE, _stored.ledMode);
}

LedMode ConfigManager::getLedMode() const {
    return sanitizeLedMode(static_cast<LedMode>(_stored.ledMode));
}

void ConfigManager::setEfIdleFloor(uint8_t floor) {
    g_efIdleFloor = static_cast<uint8_t>(constrain(static_cast<int>(floor), 0, 127));
    storageUpdate(EEPROM_EF_IDLE_FLOOR, g_efIdleFloor);
    storageUpdate(EEPROM_EF_IDLE_FLOOR_CHECK, g_efIdleFloor ^ 0xFF);
}

uint8_t ConfigManager::getEfIdleFloor() const { return g_efIdleFloor; }

// Reset configuration to defaults
void ConfigManager::resetConfiguration(std::vector<uint8_t> &potChannels,
                                       bool recordRecoveryEvent) {
    if (recordRecoveryEvent) {
        _lastRecoveryEvent = RecoveryEvent::kDefaultsLoaded;
    }
    potChannels.clear();
    for (uint8_t i = 0; i < _numPots; i++) {
        setPotChannel(i, 1);  // Default to channel 1
        setPotCCNumber(i, 0); // Default to CC 0
    }
    seedSlotEnvelopePayloads(static_cast<uint8_t>(EnvelopeFollower::LINEAR), kMinFilterFrequency,
                             1.0f);
    _stored.activeProfile = 0;
    _stored.ledMode = static_cast<uint8_t>(LedMode::Static);
    g_efIdleFloor = EF_IDLE_FLOOR_DEFAULT;
    saveConfiguration();
}

ConfigManager::RecoveryEvent ConfigManager::consumeRecoveryEvent() {
    RecoveryEvent event = _lastRecoveryEvent;
    _lastRecoveryEvent = RecoveryEvent::kNone;
    return event;
}

// Mode and ARG methods
void ConfigManager::setMode(uint8_t mode) {
    legacyArg.mode = mode;
    storageUpdate(EEPROM_ARG_MODE, legacyArg.mode);
}

uint8_t ConfigManager::getMode() const { return legacyArg.mode; }

void ConfigManager::setARGMethod(uint8_t method) {
    legacyArg.method = method;
    SlotARGConfig defaults{};
    defaults.enabled = legacyArg.enable;
    defaults.method = static_cast<ARGMethod>(legacyArg.method);
    defaults.sourceA = legacyArg.sourceA;
    defaults.sourceB = legacyArg.sourceB;
    defaults = sanitizeSlotArg(defaults);
    legacyArg.method = static_cast<uint8_t>(defaults.method);
    storageUpdate(EEPROM_ARG_METHOD, legacyArg.method);
}

uint8_t ConfigManager::getARGMethod() const { return legacyArg.method; }

void ConfigManager::setARGEnable(uint8_t enable) {
    legacyArg.enable = enable ? 1 : 0;
    storageUpdate(EEPROM_ARG_ENABLE, legacyArg.enable);
}

uint8_t ConfigManager::getARGEnable() const { return legacyArg.enable; }

void ConfigManager::setEnvelopePair(uint8_t envA, uint8_t envB) {
    uint8_t safeA = static_cast<uint8_t>(constrain(static_cast<int>(envA), 0, NUM_ENVELOPES - 1));
    uint8_t safeB = static_cast<uint8_t>(constrain(static_cast<int>(envB), 0, NUM_ENVELOPES - 1));
    if (NUM_ENVELOPES > 1 && safeA == safeB) {
        safeB = static_cast<uint8_t>((safeA + 1) % NUM_ENVELOPES);
    }
    legacyArg.sourceA = safeA;
    legacyArg.sourceB = safeB;
    storageUpdate(EEPROM_ARG_ENV_A, safeA);
    storageUpdate(EEPROM_ARG_ENV_B, safeB);
}

uint8_t ConfigManager::getEnvelopeA() const {
    int pin = envelopeAnalogPin(legacyArg.sourceA);
    if (pin < 0)
        pin = legacyArg.sourceA;
    return static_cast<uint8_t>(pin);
}

uint8_t ConfigManager::getEnvelopeB() const {
    int pin = envelopeAnalogPin(legacyArg.sourceB);
    if (pin < 0)
        pin = legacyArg.sourceB;
    return static_cast<uint8_t>(pin);
}

String ConfigManager::makeSchema() {
    String s;
    s.reserve(7900);
    s += "{\"$schema\":\"https://json-schema.org/draft/2020-12/schema\",";
    s += "\"schema_version\":";
    s += String(CONFIG_VERSION);
    s += ",\"title\":\"MOARkNOBS Runtime Configuration\",";
    s += "\"type\":\"object\",";
    s += "\"x_mn42\":{\"authority\":\"device\",";
    s += "\"configurator\":\"convenience editor for staged user input\",";
    s += "\"bridge\":\"required when USB/WebSerial transport is unavailable or host integration "
         "needs it\"},";
    s += "\"required\":[\"slots\",\"efSlots\",\"filter\",\"arg\",\"led\"],";
    s += "\"properties\":{";

    s += "\"slots\":{\"type\":\"array\",\"title\":\"Slot Configuration\",";
    s += "\"description\":\"Primary knob-to-MIDI mapping for each slot.\",\"minItems\":";
    s += String(static_cast<int>(NUM_SLOTS));
    s += ",\"maxItems\":";
    s += String(static_cast<int>(NUM_SLOTS));
    s += ",\"items\":{\"type\":\"object\",";
    s += "\"required\":[\"type\",\"midiChannel\",\"data1\",\"efIndex\",\"active\"],";
    s += "\"properties\":{";
    s += "\"type\":{\"type\":\"string\",\"title\":\"Knob -> MIDI message\",";
    s += "\"enum\":[\"OFF\",\"CC\",\"Note\",\"PitchBend\",\"ProgramChange\",\"Aftertouch\",";
    s += "\"ModWheel\",\"NRPN\",\"RPN\",\"SysEx\"]},";
    s += "\"midiChannel\":{\"type\":\"integer\",\"title\":\"MIDI "
         "channel\",\"minimum\":1,\"maximum\":16},";
    s += "\"data1\":{\"type\":\"integer\",\"title\":\"CC/Note "
         "number\",\"minimum\":0,\"maximum\":127},";
    s += "\"efIndex\":{\"type\":\"integer\",\"title\":\"Envelope follower "
         "index\",\"minimum\":-1,\"maximum\":";
    s += String(static_cast<int>(NUM_ENVELOPES - 1));
    s += "},";
    s += "\"ef\":{\"type\":\"object\",\"title\":\"Envelope Follower (EF)\",";
    s += "\"required\":[\"index\",\"filter_index\",\"filter_name\",\"frequency\",\"q\",";
    s += "\"oversample\",\"smoothing\",\"baseline\",\"gain\"],\"properties\":{";
    s += "\"index\":{\"type\":\"integer\",\"minimum\":-1,\"maximum\":";
    s += String(static_cast<int>(NUM_ENVELOPES - 1));
    s += "},";
    s += "\"filter_index\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":6},";
    s += "\"filter_name\":{\"type\":\"string\",\"enum\":[\"LINEAR\",\"OPPOSITE_LINEAR\",";
    s += "\"EXPONENTIAL\",\"RANDOM\",\"LOWPASS\",\"HIGHPASS\",\"BANDPASS\"]},";
    s += "\"frequency\":{\"type\":\"number\",\"minimum\":";
    s += String(EF_FILTER_FREQ_MIN_HZ, 1);
    s += ",\"maximum\":";
    s += String(EF_FILTER_FREQ_MAX_HZ, 1);
    s += "},";
    s += "\"q\":{\"type\":\"number\",\"minimum\":";
    s += String(EF_FILTER_Q_MIN, 2);
    s += ",\"maximum\":";
    s += String(EF_FILTER_Q_MAX, 1);
    s += "},";
    s += "\"oversample\":{\"type\":\"integer\",\"minimum\":";
    s += String(static_cast<int>(EF_OVERSAMPLE_MIN));
    s += ",\"maximum\":";
    s += String(static_cast<int>(EF_OVERSAMPLE_MAX));
    s += "},";
    s += "\"smoothing\":{\"type\":\"number\",\"minimum\":0,\"maximum\":1},";
    s += "\"baseline\":{\"type\":\"number\"},\"gain\":{\"type\":\"number\"}},";
    s += "\"additionalProperties\":true},";
    s += "\"active\":{\"type\":\"boolean\",\"title\":\"Enabled\"},";
    s += "\"sysexTemplate\":{\"type\":\"string\",\"title\":\"SysEx template\",\"maxLength\":128},";
    s += "\"arg\":{\"type\":\"object\",\"title\":\"Follower Combiner (ARG)\",";
    s += "\"required\":[\"enabled\",\"method\",\"method_name\",\"sourceA\",\"sourceB\"],";
    s += "\"properties\":{";
    s += "\"enabled\":{\"type\":\"boolean\"},";
    s += "\"method\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":";
    s += String(static_cast<int>(ARGMethod::XORR));
    s += "},";
    s += "\"method_name\":{\"type\":\"string\",\"enum\":[\"PLUS\",\"MIN\",\"PECK\",\"SHAV\",";
    s += "\"SQAR\",\"BABS\",\"TABS\",\"MULT\",\"DIVI\",\"AVG\",\"XABS\",\"MAXX\",";
    s += "\"MINN\",\"XORR\"]},";
    s += "\"sourceA\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":";
    s += String(static_cast<int>(NUM_ENVELOPES - 1));
    s += "},";
    s += "\"sourceB\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":";
    s += String(static_cast<int>(NUM_ENVELOPES - 1));
    s += "}},\"additionalProperties\":false}";
    s += "},\"additionalProperties\":false}},"; // slot properties/items/slots

    s += "\"efSlots\":{\"type\":\"array\",\"title\":\"Envelope Assignments\",\"minItems\":";
    s += String(static_cast<int>(NUM_ENVELOPES));
    s += ",\"maxItems\":";
    s += String(static_cast<int>(NUM_ENVELOPES));
    s += ",\"items\":{\"type\":\"object\",\"properties\":{";
    s += "\"slot\":{\"type\":\"integer\",\"minimum\":-1,\"maximum\":";
    s += String(static_cast<int>(NUM_SLOTS - 1));
    s += "},";
    s += "\"slots\":{\"type\":\"array\",\"items\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":";
    s += String(static_cast<int>(NUM_SLOTS - 1));
    s += "},\"uniqueItems\":true}},";
    s += "\"anyOf\":[{\"required\":[\"slot\"]},{\"required\":[\"slots\"]}],";
    s += "\"additionalProperties\":false}},";

    s += "\"filter\":{\"type\":\"object\",\"title\":\"Follower "
         "Filter\",\"required\":[\"type\",\"freq\",\"q\"],";
    s += "\"properties\":{\"type\":{\"type\":\"string\",\"enum\":[\"LINEAR\",\"OPPOSITE_LINEAR\",";
    s += "\"EXPONENTIAL\",\"RANDOM\",\"LOWPASS\",\"HIGHPASS\",\"BANDPASS\"]},";
    s += "\"freq\":{\"type\":\"number\",\"minimum\":";
    s += String(EF_FILTER_FREQ_MIN_HZ, 1);
    s += ",\"maximum\":";
    s += String(EF_FILTER_FREQ_MAX_HZ, 1);
    s += "},";
    s += "\"q\":{\"type\":\"number\",\"minimum\":";
    s += String(EF_FILTER_Q_MIN, 2);
    s += ",\"maximum\":";
    s += String(EF_FILTER_Q_MAX, 1);
    s += "},";
    s += "\"idle_floor\":{\"type\":\"integer\",\"title\":\"EF idle floor\",";
    s += "\"description\":\"Envelope levels at or below this MIDI value are clamped to zero.\",";
    s += "\"minimum\":0,\"maximum\":127,\"default\":";
    s += String(static_cast<int>(EF_IDLE_FLOOR_DEFAULT));
    s += "}},\"additionalProperties\":false},";

    s += "\"arg\":{\"type\":\"object\",\"title\":\"Follower Combiner (ARG)\",";
    s += "\"required\":[\"method\",\"a\",\"b\"],\"properties\":{";
    s += "\"method\":{\"type\":\"string\",\"enum\":[\"PLUS\",\"MIN\",\"PECK\",\"SHAV\",\"SQAR\",";
    s += "\"BABS\",\"TABS\",\"MULT\",\"DIVI\",\"AVG\",\"XABS\",\"MAXX\",\"MINN\",\"XORR\"]},";
    s += "\"a\":{\"type\":\"number\",\"minimum\":0,\"maximum\":";
    s += String(static_cast<int>(NUM_ENVELOPES - 1));
    s += "},";
    s += "\"b\":{\"type\":\"number\",\"minimum\":0,\"maximum\":";
    s += String(static_cast<int>(NUM_ENVELOPES - 1));
    s += "},";
    s += "\"enable\":{\"type\":\"boolean\"}},\"additionalProperties\":false},";

    s += "\"led\":{\"type\":\"object\",\"title\":\"LED "
         "Colors\",\"required\":[\"brightness\",\"color\"],";
    s += "\"properties\":{\"brightness\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":255},";
    s += "\"color\":{\"type\":\"string\",\"pattern\":\"^#([0-9a-fA-F]{6})$\"},";
    s += "\"mode\":{\"type\":\"string\",\"enum\":[\"STATIC\",\"PEAK_HOLD\",\"TRAIL\",";
    s += "\"CLOCK_PULSE\"],\"default\":\"STATIC\"}},\"additionalProperties\":false},";
    s += "\"envelopeMode\":{\"type\":\"string\",\"enum\":[\"LINEAR\",\"EXPONENTIAL\",\"LOG\"],";
    s += "\"default\":\"LINEAR\"}";
    s += "},\"additionalProperties\":false}";
    return s;
}

String ConfigManager::serializeAll() const {
    String output = "{ \"pots\": [";

    for (uint8_t i = 0; i < _numPots; ++i) {
        output += "{";
        output += "\"channel\": ";
        output += _stored.potChannels.at(i);
        output += ", \"cc\": ";
        output += _stored.potCCNumbers.at(i);
        output += "}";

        if (i < _numPots - 1) {
            output += ",";
        }
    }

    output += "], \"slots\": [";
    for (uint8_t i = 0; i < NUM_SLOTS; ++i) {
        SlotEnvelopePayload payload =
            sanitizeEnvelopePayload(settingsToPayload(slots[i].efSettings));
        output += "{";
        output += "\"index\": ";
        output += i;
        output += ", \"ef_payload\": {";
        output += "\"type\": ";
        output += payload.filterType;
        output += ", \"freq\": ";
        output += String(payload.frequency, 2);
        output += ", \"q\": ";
        output += String(payload.q, 2);
        output += "}, \"arg\": {";
        SlotARGConfig arg = sanitizeSlotArg(slots[i].arg);
        output += "\"enabled\": ";
        output += arg.enabled;
        output += ", \"method\": ";
        output += static_cast<uint8_t>(arg.method);
        output += ", \"sourceA\": ";
        output += arg.sourceA;
        output += ", \"sourceB\": ";
        output += arg.sourceB;
        output += "}}";
        if (i < NUM_SLOTS - 1) {
            output += ",";
        }
    }
    output += "] }";
    return output;
}

void ConfigManager::saveMIDISlots(const MIDISlot *slots, size_t count) {
    if (slots == nullptr || count == 0) {
        return;
    }
    // Clamp to maximum number of slots to avoid overflow
    if (count > 42) {
        count = 42;
    }
    for (size_t i = 0; i < count; ++i) {
        saveSlot(static_cast<uint8_t>(i), slots[i]);
    }
}

void ConfigManager::loadMIDISlots(MIDISlot *slots, size_t count) {
    if (slots == nullptr || count == 0) {
        return;
    }
    if (count > 42) {
        count = 42;
    }
    for (size_t i = 0; i < count; ++i) {
        loadSlot(static_cast<uint8_t>(i), slots[i]);
    }
}

SlotEnvelopePayload ConfigManager::getSlotEnvelopePayload(uint8_t idx) const {
    if (idx >= slots.size()) {
        SlotEnvelopePayload fallback{};
        return sanitizeEnvelopePayload(fallback);
    }
    return sanitizeEnvelopePayload(settingsToPayload(slots[idx].efSettings));
}

void ConfigManager::setSlotEnvelopePayload(uint8_t idx, const SlotEnvelopePayload &payload) {
    if (idx >= slots.size()) {
        return;
    }
    SlotEnvelopePayload sanitized = sanitizeEnvelopePayload(payload);
    MIDISlot &slot = slots[idx];
    applyPayloadToSettings(sanitized, slot.efSettings);
    slot.efSettings = sanitizeEfSettings(slot.efSettings);
    slot.ef = sanitizeEfRuntime(slot.ef);
    saveSlot(idx, slots[idx]);
}

SlotEnvelopePayload ConfigManager::sanitizeEnvelopePayload(const SlotEnvelopePayload &payload) {
    return sanitizeEnvelopePayloadImpl(payload);
}

SlotEnvelopePayload ConfigManager::persistFilterTail(const SlotEnvelopePayload &payload) {
    return persistFilterTailImpl(payload);
}

// loadLegacyARGSettings, migrateLegacyARGSettings, slotLooksSane,
// sanitizeSlotArena, wipeSlotRegion, migrateLegacySlotPayloads,
// seedSlotEnvelopePayloads, sanitizeEnvelopePayload, and wipeProfileBlocks
// are now implemented in SchemaMigration.cpp.

bool ConfigManager::handleCommand(const String &command) {
    if (command.startsWith("CAL_ENVS")) {
        for (auto &ef : envelopeFollowers) {
            ef.calibrate();
        }
        LOG_PRINTLN("{\"type\":\"response\",\"status\":\"ok\"}");
        return true;
    } else if (command.startsWith("GET_FILTER")) {
        const uint8_t efType = storageRead(EEPROM_ENVELOPE_TYPES);
        (void)efType; // keep the compiler chill
        float freq, q;
        storageGet(EEPROM_FILTER_FREQ, freq);
        storageGet(EEPROM_FILTER_Q, q);
        LOG_PRINTLN("{\"type\":\"response\",\"message\":\"GET_FILTER deprecated\"}");
        return true;
    } else if (command.startsWith("SET_FILTER")) {
        int firstComma = command.indexOf(',');
        int secondComma = command.indexOf(',', firstComma + 1);
        if (firstComma == -1 || secondComma == -1) {
            LOG_PRINTLN("{\"type\":\"response\",\"status\":\"error\"}");
            return true;
        }
        uint8_t efType = command.substring(10, firstComma).toInt();
        float freq = command.substring(firstComma + 1, secondComma).toFloat();
        float q = command.substring(secondComma + 1).toFloat();
        seedSlotEnvelopePayloads(efType, freq, q);
        LOG_PRINTLN("{\"type\":\"response\",\"status\":\"ok\"}");
        return true;
    } else if (command.startsWith("GET_SLOT_FILTER")) {
        int slotIndex = command.substring(16).toInt();
        if (slotIndex < 0 || slotIndex >= static_cast<int>(NUM_SLOTS)) {
            LOG_PRINTLN("{\"type\":\"response\",\"status\":\"error\"}");
            return true;
        }
        LOG_PRINTLN("{\"type\":\"response\",\"message\":\"GET_SLOT_FILTER deprecated\"}");
        return true;
    } else if (command.startsWith("SET_SLOT_FILTER")) {
        int first = command.indexOf(',');
        int second = command.indexOf(',', first + 1);
        int third = command.indexOf(',', second + 1);
        if (first == -1 || second == -1 || third == -1) {
            LOG_PRINTLN("{\"type\":\"response\",\"status\":\"error\"}");
            return true;
        }
        int slotIndex = command.substring(16, first).toInt();
        int rawType = command.substring(first + 1, second).toInt();
        float freq = command.substring(second + 1, third).toFloat();
        float q = command.substring(third + 1).toFloat();
        if (slotIndex < 0 || slotIndex >= static_cast<int>(NUM_SLOTS)) {
            LOG_PRINTLN("{\"type\":\"response\",\"status\":\"error\"}");
            return true;
        }
        SlotEnvelopePayload payload = getSlotEnvelopePayload(static_cast<uint8_t>(slotIndex));
        payload.filterType = static_cast<uint8_t>(rawType);
        payload.frequency = freq;
        payload.q = q;
        setSlotEnvelopePayload(static_cast<uint8_t>(slotIndex), payload);
        LOG_PRINTLN("{\"type\":\"response\",\"status\":\"ok\"}");
        return true;
    } else if (command.startsWith("GET_ARGPAIR")) {
        LOG_PRINTLN("{\"type\":\"response\",\"message\":\"GET_ARGPAIR deprecated\"}");
        return true;
    } else if (command.startsWith("SET_ARGPAIR")) {
        int first = command.indexOf(',');
        int second = command.indexOf(',', first + 1);
        if (first == -1 || second == -1) {
            LOG_PRINTLN("{\"type\":\"response\",\"status\":\"error\"}");
            return true;
        }
        uint8_t enable = command.substring(11, first).toInt();
        uint8_t envA = command.substring(first + 1, second).toInt();
        uint8_t envB = command.substring(second + 1).toInt();
        setARGEnable(enable);
        setEnvelopePair(envA, envB);

        int idxA = envelopeIndexFromAnalogPin(envA);
        if (idxA < 0)
            idxA = constrain(envA, 0, NUM_ENVELOPES - 1);
        int idxB = envelopeIndexFromAnalogPin(envB);
        if (idxB < 0)
            idxB = constrain(envB, 0, NUM_ENVELOPES - 1);

        for (uint8_t slotIndex = 0; slotIndex < NUM_SLOTS; ++slotIndex) {
            MIDISlot &slot = getSlot(slotIndex);
            slot.arg.enabled = enable ? 1 : 0;
            slot.arg.sourceA = static_cast<uint8_t>(idxA);
            slot.arg.sourceB = static_cast<uint8_t>(idxB);
            saveSlot(slotIndex, slot);
        }
        LOG_PRINTLN("{\"type\":\"response\",\"status\":\"ok\"}");
        return true;
    }
    return false;
}

#if defined(UNIT_TEST) || defined(FULL_SYSTEM_COMBINED)
#include "SystemTestShim.h"
#include "TestHelpers.h"

// Ensure the manager can resurrect configuration from the backup copy
// after the primary EEPROM header gets nuked.
void test_eeprom_recovery_after_power_cycle() {
    ConfigManager cfg(NUM_POTS, NUM_BUTTONS);
    cfg.setPotChannel(0, 9);
    cfg.setPotCCNumber(0, 77);
    cfg.saveConfiguration();

    // Mirror primary data into the backup region and wreck the primary header
    storageUpdate(EEPROM_MAGIC_ADDRESS + 2, (EEPROM_MAGIC_BACKUP >> 8) & 0xFF);
    storageUpdate(EEPROM_MAGIC_ADDRESS + 3, EEPROM_MAGIC_BACKUP & 0xFF);
    storageUpdate(EEPROM_BACKUP_START + EEPROM_POT_CHANNELS, 9);
    storageUpdate(EEPROM_BACKUP_START + EEPROM_POT_CC, 77);
    storageUpdate(EEPROM_MAGIC_ADDRESS, 0x00);
    storageUpdate(EEPROM_MAGIC_ADDRESS + 1, 0x00);

    ConfigManager rebooted(NUM_POTS, NUM_BUTTONS);
    std::vector<uint8_t> pots;
    bool ok = rebooted.loadConfiguration(pots);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_UINT8(9, pots[0]);
    TEST_ASSERT_EQUAL_UINT8(77, rebooted.getPotCCNumber(0));
}

// Baseline calibration should make it through a simulated reboot.
void test_calibration_offsets_survive_power_cycle() {
    auto pm = createPotentiometerManager();
    std::vector<EnvelopeFollower> envs = {EnvelopeFollower(A0, &pm, 0)};
    std::map<int, MIDISlot::EfSettings> mapping;
    MIDISlot::EfSettings settings;
    settings.followerIndex = 0;
    mapping.emplace(0, settings);

    envs[0].setBaseline(0.42f);
    ConfigManager cfg(NUM_POTS, NUM_BUTTONS);
    cfg.saveEnvelopeSettings(mapping, envs);

    // Pretend the board restarted – wipe RAM and reload from EEPROM
    for (int i = 0; i < NUM_ENVELOPES; ++i) {
        envelopeConfig.baselines[i] = 0.0f;
    }
    std::vector<EnvelopeFollower> fresh = {EnvelopeFollower(A0, &pm, 0)};
    std::map<int, MIDISlot::EfSettings> mapping2;
    bool ok = cfg.loadEnvelopeSettings(mapping2, fresh);

    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.42f, fresh[0].getBaseline());
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.42f, envelopeConfig.baselines[0]);
}

void test_high_index_envelope_assignment_survives_reload() {
    auto pm = createPotentiometerManager();
    auto envs = createEnvelopeFollowers(&pm);

    for (size_t i = 0; i < envs.size(); ++i) {
        envs[i].setBaseline(0.1f * static_cast<float>(i + 1));
    }

    std::map<int, MIDISlot::EfSettings> mapping;
    const int highPot = NUM_POTS - 1;
    const int assignedEnv = static_cast<int>(envs.size()) - 1;
    MIDISlot::EfSettings assignedSettings{};
    assignedSettings.followerIndex = static_cast<int8_t>(assignedEnv);
    mapping.emplace(highPot, assignedSettings);

    ConfigManager cfg(NUM_POTS, NUM_BUTTONS);
    cfg.saveEnvelopeSettings(mapping, envs);

    mapping.clear();
    auto reloaded = createEnvelopeFollowers(&pm);
    bool ok = cfg.loadEnvelopeSettings(mapping, reloaded);

    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_UINT(1u, mapping.size());

    auto highPotIt = mapping.find(highPot);
    TEST_ASSERT_TRUE(highPotIt != mapping.end());
    TEST_ASSERT_EQUAL_INT(assignedEnv, highPotIt->second.followerIndex);

    if (NUM_POTS > 1) {
        int unassignedPot = (highPot == 0) ? 1 : 0;
        auto unassignedIt = mapping.find(unassignedPot);
        TEST_ASSERT_TRUE(unassignedIt == mapping.end());
    }
}
#endif
