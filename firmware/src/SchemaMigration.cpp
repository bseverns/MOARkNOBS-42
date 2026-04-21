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
#include "EnvelopeFollower.h"
#include "ARGMixer.h"
#include "storage/EepromStorageBackend.h"
#include "storage/LittleFsStorageBackend.h"
#include <cmath>
#include <cstddef>
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

template <typename T> void storagePut(int address, const T &value) {
    ConfigManager::getStorageBackend()->writeBytes(address, &value, sizeof(T));
}

constexpr uint16_t kLegacyConfigVersion = 0x0003;
constexpr float kMinFilterFrequency = 20.0f;
constexpr float kMaxFilterFrequency = 5000.0f;
constexpr float kMinFilterQ = 0.5f;
constexpr float kMaxFilterQ = 4.0f;

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

    uint16_t storedVersion = 0;
    storageGet(EEPROM_CONFIG_VERSION, storedVersion);

    if (storedVersion == CONFIG_VERSION) {
        return;
    }

    if (storedVersion == 0x0003) {
        struct LegacyMIDISlotV3 {
            MIDIMessageType type;
            uint8_t midiChannel;
            uint8_t data1;
            uint8_t efIndex;
            uint8_t active;
            uint8_t arpNote;
            uint8_t sysexLength;
            std::array<uint8_t, SysExTemplate::kMaxLength> sysexTemplate;
        };
        static_assert(sizeof(LegacyMIDISlotV3) == 23, "Legacy MIDISlot size mismatch");

        std::array<LegacyMIDISlotV3, NUM_SLOTS> legacySlots{};
        for (uint8_t i = 0; i < NUM_SLOTS; ++i) {
            const int legacyAddress =
                static_cast<int>(EEPROM_SLOT_BASE + i * sizeof(LegacyMIDISlotV3));
            storageGet(legacyAddress, legacySlots[i]);
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

        for (uint8_t i = 0; i < NUM_SLOTS; ++i) {
            MIDISlot upgraded{};
            upgraded.type = legacySlots[i].type;
            upgraded.midiChannel = legacySlots[i].midiChannel;
            upgraded.data1 = legacySlots[i].data1;
            upgraded.ef.followerIndex = static_cast<int8_t>(legacySlots[i].efIndex);
            upgraded.active = legacySlots[i].active != 0;
            upgraded.arpNote = legacySlots[i].arpNote;
            upgraded.sysexLength = legacySlots[i].sysexLength;
            upgraded.sysexTemplate = legacySlots[i].sysexTemplate;
            upgraded.arg = defaults;
            saveSlot(i, upgraded);
        }

        storagePut(EEPROM_CONFIG_VERSION, static_cast<uint16_t>(CONFIG_VERSION));
    }
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
    return true;
}

void ConfigManager::sanitizeSlotArena() {
    migrateLegacyARGSettings();
    loadLegacyARGSettings();

    uint16_t storedVersion = 0;
    storageGet(EEPROM_CONFIG_VERSION, storedVersion);

    migrateLegacyARGSettings();

    if (storedVersion == CONFIG_VERSION) {
        maybeRescueFilterTailFromLegacy();
        MIDISlot candidate{};
        storageGet(static_cast<int>(EEPROM_SLOT_BASE), candidate);
        if (!slotLooksSane(candidate)) {
            wipeSlotRegion();
            wipeProfileBlocks();
        }
        return;
    }

    if (storedVersion == 0 || storedVersion > CONFIG_VERSION) {
        wipeSlotRegion();
        wipeProfileBlocks();
        storagePut(EEPROM_CONFIG_VERSION, static_cast<uint16_t>(CONFIG_VERSION));
        return;
    }

    migrateLegacySlotPayloads(storedVersion);
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

void ConfigManager::migrateLegacySlotPayloads(uint16_t storedVersion) {
    if (storedVersion != kLegacyConfigVersion && storedVersion != 0x0004) {
        wipeSlotRegion();
        wipeProfileBlocks();
        return;
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

    if (storedVersion == kLegacyConfigVersion) {
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

        persistFilterTail(sanitizedPayload);

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
            storagePut(upgradedAddress, upgraded);
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
            storagePut(upgradedAddress, upgraded);
        }

        MIDISlot first{};
        loadSlot(0, first);
        persistFilterTail(settingsToPayload(first.efSettings));
    }

    storageUpdate(EEPROM_ARG_ENABLE, legacyArg.enable);
    storageUpdate(EEPROM_ARG_METHOD, legacyArg.method);
    storageUpdate(EEPROM_ARG_ENV_A, legacyArg.sourceA);
    storageUpdate(EEPROM_ARG_ENV_B, legacyArg.sourceB);
    storagePut(EEPROM_CONFIG_VERSION, static_cast<uint16_t>(CONFIG_VERSION));

    slots.fill({});
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

void ConfigManager::wipeProfileBlocks() {
    for (uint8_t id = 1; id < NUM_PROFILES; ++id) {
        const uint16_t base = EEPROM_PROFILE_START(id);
        for (uint16_t offset = 0; offset < EEPROM_PROFILE_BLOCK_SIZE; ++offset) {
            storageUpdate(static_cast<int>(base + offset), 0x00);
        }
    }
    for (uint8_t id = 0; id < NUM_PROFILES; ++id) {
        const uint16_t base = EEPROM_PROFILE_SETTINGS_START(id);
        for (uint16_t offset = 0; offset < EEPROM_PROFILE_SETTINGS_BLOCK_SIZE; ++offset) {
            storageUpdate(static_cast<int>(base + offset), 0x00);
        }
    }
}
