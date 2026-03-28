#include "Protocol.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <cmath>
#include <imxrt.h>
#include <cctype>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cstdio>
#include <array>
#include <algorithm>
#include <EEPROM.h>

#include "ARGMixer.h"
#include "CommandQueue.h"
#include "ConfigManager.h"
#include "EfSettingsUtils.h"
#include "FirmwareState.h"
#include "Globals.h"
#include "EnvelopeFollower.h"
#include "Log.h"
#include "version.h"
#include "Modes.h"
#include "Utility.h"
#include "SysExTemplate.h"
#include "protocol/ManifestReport.h"

#if defined(UNIT_TEST)
bool testOnly_parseSlotType(JsonVariantConst typeField, JsonVariantConst typeNameField,
                            MIDIMessageType &type) {
    return parseSlotType(typeField, typeNameField, type);
}

bool testOnly_parseSysExTemplateField(JsonVariantConst value, MIDISlot &slot, String &error) {
    return parseSysExTemplateField(value, slot, error);
}

uint8_t testOnly_buildSysExPayload(const MIDISlot &slot, uint16_t rawValue, uint8_t *dest,
                                   size_t capacity) {
    return buildSysExPayload(slot, rawValue, dest, capacity);
}
#endif

const char *envelopeModeName(uint8_t mode);
EnvelopeFollower::ARG_Method toFollowerArgMethod(ARGMethod method);

namespace {
Utility::BulkConfigAssembler bulkConfigAssembler;
uint32_t lastAckSequence = 0;
String lastAckChecksum;

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

void syncPotentiometerMappingsFromConfig() {
    potChannels.clear();
    for (uint8_t i = 0; i < configManager.getNumPots(); ++i) {
        const uint8_t channel = constrain(configManager.getPotChannel(i), 1, 16);
        const uint8_t cc = constrain(configManager.getPotCCNumber(i), 0, 127);
        configManager.setPotChannel(i, channel);
        configManager.setPotCCNumber(i, cc);
        potentiometerManager.setChannel(i, channel);
        potentiometerManager.setCCNumber(i, cc);
        potChannels.push_back(channel);
    }
}

ProfileData defaultProfileSnapshot() {
    ProfileData profile{};
    for (uint8_t i = 0; i < NUM_SLOTS; ++i) {
        profile.slots[i].midiChannel = 1;
    }
    return profile;
}

bool saveCurrentProfileSlot(uint8_t id) {
    if (id >= NUM_PROFILES) {
        return false;
    }
    g_activeProfile = id;
    configManager.setActiveProfile(id);
    configManager.saveProfile(id);
    configManager.saveEnvelopeSettings(potToEnvelopeMap, envelopeFollowers);
    return configManager.saveProfileSettings(id, captureProfileSnapshot());
}

bool loadProfileSlot(uint8_t id) {
    if (id >= NUM_PROFILES) {
        return false;
    }

    ProfileData profile{};
    const bool stored = configManager.loadProfileSettings(id, profile);
    g_activeProfile = id;
    configManager.setActiveProfile(id);

    if (stored) {
        configManager.loadProfile(id);
    } else {
        profile = defaultProfileSnapshot();
        for (uint8_t i = 0; i < configManager.getNumPots(); ++i) {
            configManager.setPotChannel(i, 1);
            configManager.setPotCCNumber(i, 0);
        }
        configManager.saveConfiguration();
    }

    syncPotentiometerMappingsFromConfig();
    applyProfileSnapshot(profile, true);
    refreshEfVoicesFromConfig();

    if (!stored) {
        configManager.saveProfile(id);
        configManager.saveProfileSettings(id, profile);
    }
    return true;
}

bool resetProfileSlot(uint8_t id) {
    if (id >= NUM_PROFILES) {
        return false;
    }

    const ProfileData profile = defaultProfileSnapshot();
    g_activeProfile = id;
    configManager.setActiveProfile(id);
    for (uint8_t i = 0; i < configManager.getNumPots(); ++i) {
        configManager.setPotChannel(i, 1);
        configManager.setPotCCNumber(i, 0);
    }
    configManager.saveConfiguration();
    syncPotentiometerMappingsFromConfig();
    applyProfileSnapshot(profile, true);
    refreshEfVoicesFromConfig();
    configManager.saveProfile(id);
    return configManager.saveProfileSettings(id, profile);
}
} // namespace

namespace SceneStorage {
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
    uint8_t ledMode = static_cast<uint8_t>(LedMode::Static);
    uint8_t ledR = 0;
    uint8_t ledG = 0;
    uint8_t ledB = 0;
    uint8_t filterType = static_cast<uint8_t>(EnvelopeFollower::LINEAR);
    float filterFrequency = 20.0f;
    float filterQ = 1.0f;
    std::array<float, NUM_ENVELOPES> baselines{};
};

struct SceneEntry {
    char name[16] = {0};
    ConfigState state{};
};

struct MacroRecord {
    uint16_t version = 1;
    uint16_t crc = 0;
    uint8_t occupied = 0;
    ConfigState state{};
};

struct SceneRecord {
    uint16_t version = 1;
    uint16_t crc = 0;
    uint8_t occupied = 0;
    char name[16] = {0};
    ConfigState state{};
};

constexpr uint8_t kSceneSlotCount = 6;
constexpr uint16_t kStorageVersion = 1;
constexpr uint16_t kMacroStorageAddress =
    EEPROM_PROFILE_SETTINGS_BASE + NUM_PROFILES * EEPROM_PROFILE_SETTINGS_BLOCK_SIZE;
constexpr uint16_t kSceneStorageBase =
    static_cast<uint16_t>(kMacroStorageAddress + sizeof(MacroRecord));

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

ConfigState captureConfigState() {
    ConfigState snapshot{};
    for (uint8_t i = 0; i < configManager.getNumPots(); ++i) {
        snapshot.potChannels[i] = constrain(configManager.getPotChannel(i), 1, 16);
        snapshot.potCCNumbers[i] = constrain(configManager.getPotCCNumber(i), 0, 127);
    }
    snapshot.slots = configManager.getSlots();
    snapshot.argEnabled = configManager.getARGEnable();
    snapshot.argMethod = configManager.getARGMethod();
    int envA = static_cast<int>(configManager.getEnvelopeA());
    int envB = static_cast<int>(configManager.getEnvelopeB());
    if (envA >= NUM_ENVELOPES) {
        int converted = envelopeIndexFromAnalogPin(envA);
        envA = converted >= 0 ? converted : constrain(envA, 0, NUM_ENVELOPES - 1);
    }
    if (envB >= NUM_ENVELOPES) {
        int converted = envelopeIndexFromAnalogPin(envB);
        envB = converted >= 0 ? converted : constrain(envB, 0, NUM_ENVELOPES - 1);
    }
    snapshot.argSourceA = static_cast<uint8_t>(envA);
    snapshot.argSourceB = static_cast<uint8_t>(envB);
    snapshot.envelopeMode = configManager.getMode();
    snapshot.ledBrightness = ledManager.getBrightness();
    snapshot.ledMode = static_cast<uint8_t>(configManager.getLedMode());
    const CRGB color = ledManager.getColor();
    snapshot.ledR = color.r;
    snapshot.ledG = color.g;
    snapshot.ledB = color.b;
    if (!envelopeFollowers.empty()) {
        snapshot.filterType = static_cast<uint8_t>(envelopeFollowers.front().getFilterType());
    }
    EEPROM.get(EEPROM_FILTER_FREQ, snapshot.filterFrequency);
    EEPROM.get(EEPROM_FILTER_Q, snapshot.filterQ);
    for (uint8_t i = 0; i < NUM_ENVELOPES; ++i) {
        snapshot.baselines[i] = envelopeConfig.baselines[i];
    }
    return snapshot;
}

void applyConfigState(const ConfigState &state, bool persist) {
    const auto slotsState = state.slots;
    for (uint8_t i = 0; i < NUM_SLOTS; ++i) {
        MIDISlot slot = slotsState[i];
        slot.midiChannel = constrain(slot.midiChannel, 1, 16);
        slot.data1 = constrain(slot.data1, 0, 127);
        configManager.saveSlot(i, slot);
    }

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

    const EnvelopeFollower::ARG_Method followerMethod = toFollowerArgMethod(
        static_cast<ARGMethod>(constrain(state.argMethod, 0, static_cast<int>(ARGMethod::XORR))));
    const bool argEnabled = state.argEnabled != 0;
    envelopeFollowMode = argEnabled;
    configManager.setMode(state.envelopeMode);
    configManager.setARGEnable(argEnabled ? 1 : 0);
    configManager.setARGMethod(
        static_cast<uint8_t>(constrain(state.argMethod, 0, static_cast<int>(ARGMethod::XORR))));
    configManager.setEnvelopePair(state.argSourceA, state.argSourceB);
    potentiometerManager.setArgEnvelopePair(state.argSourceA, state.argSourceB);
    updateEnvelopeModeLabel(envelopeModeName(state.envelopeMode));

    SlotEnvelopePayload tailPayload{};
    tailPayload.filterType = static_cast<uint8_t>(
        constrain(state.filterType, 0, static_cast<int>(EnvelopeFollower::BANDPASS)));
    tailPayload.frequency = constrain(state.filterFrequency, 20.0f, 5000.0f);
    tailPayload.q = constrain(state.filterQ, 0.5f, 4.0f);
    SlotEnvelopePayload sanitizedTail = configManager.persistFilterTail(tailPayload);
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

    const CRGB color(state.ledR, state.ledG, state.ledB);
    ledManager.setBrightness(state.ledBrightness);
    ledManager.setColor(color);
    configManager.saveLEDSettings(state.ledBrightness, color);
    LedMode ledMode = static_cast<LedMode>(state.ledMode);
    configManager.setLedMode(ledMode);
    ledAnimator.setMode(ledMode);

    if (persist) {
        configManager.saveEnvelopeSettings(potToEnvelopeMap, envelopeFollowers);
    }
    refreshEfVoicesFromConfig();
}

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
        EEPROM.get(sceneSlotAddress(slot), record);
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
    EEPROM.put(sceneSlotAddress(slot), record);
    return true;
}

bool loadSceneSlot(uint8_t slot, SceneEntry &entry) {
    if (slot >= kSceneSlotCount) {
        return false;
    }
    SceneRecord record{};
    EEPROM.get(sceneSlotAddress(slot), record);
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
    EEPROM.get(sceneSlotAddress(slot), record);
    return recordValid(record);
}

bool macroSnapshotAvailable() {
    MacroRecord record{};
    EEPROM.get(kMacroStorageAddress, record);
    return recordValid(record);
}

bool loadMacroSnapshot(ConfigState &state) {
    MacroRecord record{};
    EEPROM.get(kMacroStorageAddress, record);
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
    EEPROM.put(kMacroStorageAddress, record);
    return true;
}
} // namespace SceneStorage

template <size_t Capacity> static void sendJsonResponse(const StaticJsonDocument<Capacity> &doc) {
    String payload;
    serializeJson(doc, payload);
    LOG_PRINTLN(payload);
}

static bool handleSceneJsonCommand(const String &command) {
    if (command.length() == 0 || command[0] != '{') {
        return false;
    }
    StaticJsonDocument<512> request;
    DeserializationError err = deserializeJson(request, command);
    if (err) {
        return false;
    }
    const char *cmd = request["cmd"] | nullptr;
    if (!cmd) {
        return false;
    }

    if (std::strcmp(cmd, "GET_SCENES") == 0) {
        SceneStorage::SceneInfo scenes[SceneStorage::kSceneSlotCount];
        uint8_t count = SceneStorage::listScenes(scenes, SceneStorage::kSceneSlotCount);
        StaticJsonDocument<768> response;
        response["cmd"] = "GET_SCENES";
        JsonArray array = response.createNestedArray("scenes");
        for (uint8_t idx = 0; idx < count; ++idx) {
            JsonObject scene = array.createNestedObject();
            scene["slot"] = scenes[idx].slot;
            scene["name"] = scenes[idx].name;
            scene["available"] = scenes[idx].available;
        }
        sendJsonResponse(response);
        return true;
    }

    if (std::strcmp(cmd, "SAVE_SCENE") == 0 || std::strcmp(cmd, "RECALL_SCENE") == 0) {
        const int slotValue = request["slot"].is<int>() ? request["slot"].as<int>() : -1;
        if (slotValue < 0 || slotValue >= SceneStorage::kSceneSlotCount) {
            StaticJsonDocument<256> response;
            response["cmd"] = cmd;
            response["scene_slot"] = slotValue;
            response["success"] = false;
            response["scene_error"] = "Invalid slot";
            sendJsonResponse(response);
            return true;
        }

        if (std::strcmp(cmd, "SAVE_SCENE") == 0) {
            const char *name = request["name"] | nullptr;
            SceneStorage::ConfigState snapshot = SceneStorage::captureConfigState();
            bool saved =
                SceneStorage::saveSceneSlot(static_cast<uint8_t>(slotValue), snapshot, name);
            SceneStorage::SceneEntry entry{};
            SceneStorage::loadSceneSlot(static_cast<uint8_t>(slotValue), entry);
            StaticJsonDocument<384> response;
            response["cmd"] = "SAVE_SCENE";
            response["scene_saved"] = saved;
            response["scene_slot"] = slotValue;
            response["scene_name"] = entry.name;
            response["scene_available"] =
                SceneStorage::sceneSlotAvailable(static_cast<uint8_t>(slotValue));
            if (!saved) {
                response["scene_error"] = "Snapshot save failed";
            }
            sendJsonResponse(response);
            return true;
        }

        SceneStorage::SceneEntry entry{};
        bool loaded = SceneStorage::loadSceneSlot(static_cast<uint8_t>(slotValue), entry);
        StaticJsonDocument<384> response;
        response["cmd"] = "RECALL_SCENE";
        response["scene_slot"] = slotValue;
        response["scene_name"] = entry.name;
        response["scene_available"] = loaded;
        if (loaded) {
            SceneStorage::applyConfigState(entry.state, true);
            response["scene_recalled"] = true;
        } else {
            response["scene_recalled"] = false;
            response["scene_error"] = "No snapshot stored in this slot";
        }
        sendJsonResponse(response);
        return true;
    }

    return false;
}

void initializeProtocol() {
    // Boot banner + reset diagnostics are emitted early so host tooling can log reset cause and
    // brownout history before config RPCs begin.
    Serial.begin(SERIAL_BAUD);
    Serial.printf("MN42 FW %s %s\n", FW_VERSION_STR, GIT_SHA_STR);
    g_resetCause = SRC_SRSR;
    EEPROM.get(EEPROM_BROWNOUT_COUNT, g_brownoutCount);
    if (g_brownoutCount == 0xFFFF) {
        g_brownoutCount = 0;
        EEPROM.put(EEPROM_BROWNOUT_COUNT, g_brownoutCount);
    }
    if (g_resetCause & 0x40) {
        g_brownoutCount++;
        EEPROM.put(EEPROM_BROWNOUT_COUNT, g_brownoutCount);
    }
    Serial.printf("MN42 FW %s schema %04X UID %08lX%08lX%08lX%08lX\n", FW_VERSION_STR,
                  CONFIG_VERSION, HW_OCOTP_CFG0, HW_OCOTP_CFG1, HW_OCOTP_CFG2, HW_OCOTP_CFG3);
    Serial.printf("Reset 0x%08lX Brownouts %u\n", g_resetCause, g_brownoutCount);
    configManager.begin(potChannels);
    potentiometerManager.attachConfigManager(configManager);
    configManager.loadMIDISlots(&configManager.getSlot(0), NUM_SLOTS);
}

void clearSysExTemplate(MIDISlot &slot) {
    slot.sysexLength = 0;
    slot.sysexTemplate.fill(0);
}

bool parseSysExTemplateField(JsonVariantConst value, MIDISlot &slot, String &error) {
    if (value.isNull()) {
        clearSysExTemplate(slot);
        return true;
    }
    const char *raw = value.as<const char *>();
    if (!raw || raw[0] == '\0') {
        clearSysExTemplate(slot);
        return true;
    }
    if (SysExTemplate::parse(raw, slot.sysexTemplate, slot.sysexLength, error)) {
        return true;
    }
    clearSysExTemplate(slot);
    return false;
}

String formatSysExTemplate(const MIDISlot &slot) {
    if (slot.sysexLength == 0) {
        return String();
    }
    return SysExTemplate::format(slot.sysexTemplate, slot.sysexLength);
}

uint8_t buildSysExPayload(const MIDISlot &slot, uint16_t rawValue, uint8_t *dest,
                          std::size_t capacity) {
    const uint8_t value7 = Utility::mapToMidiValue(static_cast<int>(rawValue));
    const uint16_t value14 = Utility::mapTo14Bit(static_cast<int>(rawValue));
    if (slot.sysexLength >= 2) {
        uint8_t rendered = SysExTemplate::render(slot.sysexTemplate, slot.sysexLength, value7,
                                                 value14, dest, capacity);
        if (rendered > 0) {
            return rendered;
        }
    }
    if (capacity < 4) {
        return 0;
    }
    dest[0] = 0xF0;
    dest[1] = slot.data1;
    dest[2] = value7;
    dest[3] = 0xF7;
    return 4;
}

const char *midiMessageTypeName(MIDIMessageType type) {
    switch (type) {
    case MIDIMessageType::OFF:
        return "OFF";
    case MIDIMessageType::CC:
        return "CC";
    case MIDIMessageType::Note:
        return "NOTE";
    case MIDIMessageType::PitchBend:
        return "PITCH_BEND";
    case MIDIMessageType::ProgramChange:
        return "PROGRAM";
    case MIDIMessageType::Aftertouch:
        return "AFTERTOUCH";
    case MIDIMessageType::ModWheel:
        return "MOD_WHEEL";
    case MIDIMessageType::NRPN:
        return "NRPN";
    case MIDIMessageType::RPN:
        return "RPN";
    case MIDIMessageType::SysEx:
        return "SYSEX";
    }
    return "UNKNOWN";
}

const char *envelopeFilterName(EnvelopeFollower::FilterType type) {
    switch (type) {
    case EnvelopeFollower::LINEAR:
        return "LINEAR";
    case EnvelopeFollower::OPPOSITE_LINEAR:
        return "OPPOSITE_LINEAR";
    case EnvelopeFollower::EXPONENTIAL:
        return "EXPONENTIAL";
    case EnvelopeFollower::RANDOM:
        return "RANDOM";
    case EnvelopeFollower::LOWPASS:
        return "LOWPASS";
    case EnvelopeFollower::HIGHPASS:
        return "HIGHPASS";
    case EnvelopeFollower::BANDPASS:
        return "BANDPASS";
    default:
        return "CUSTOM";
    }
}

const char *efFilterLabel(MIDISlot::EfSettings::FilterType type) {
    using Filter = MIDISlot::EfSettings::FilterType;
    switch (type) {
    case Filter::Linear:
        return "LINEAR";
    case Filter::OppositeLinear:
        return "OPPOSITE_LINEAR";
    case Filter::Exponential:
        return "EXPONENTIAL";
    case Filter::Random:
        return "RANDOM";
    case Filter::Lowpass:
        return "LOWPASS";
    case Filter::Highpass:
        return "HIGHPASS";
    case Filter::Bandpass:
        return "BANDPASS";
    }
    return "LINEAR";
}

const char *argMethodName(uint8_t method) {
    static constexpr const char *kNames[] = {"PLUS", "MIN",  "PECK", "SHAV", "SQAR",
                                             "BABS", "TABS", "MULT", "DIVI", "AVG",
                                             "XABS", "MAXX", "MINN", "XORR"};
    if (method < (sizeof(kNames) / sizeof(kNames[0]))) {
        return kNames[method];
    }
    return "UNKNOWN";
}

const char *envelopeModeName(uint8_t mode) { return (mode != 0) ? "ARG" : "SEF"; }

bool equalsIgnoreCase(const char *lhs, const char *rhs) {
    if (!lhs || !rhs)
        return false;
    while (*lhs && *rhs) {
        if (tolower(static_cast<unsigned char>(*lhs)) != tolower(static_cast<unsigned char>(*rhs)))
            return false;
        ++lhs;
        ++rhs;
    }
    return *lhs == '\0' && *rhs == '\0';
}

bool parseMIDIType(const char *label, MIDIMessageType &type) {
    if (!label)
        return false;
    struct Entry {
        const char *legacy;
        const char *canonical;
        const char *alt;
        MIDIMessageType value;
    };
    static constexpr Entry kMap[] = {
        {"OFF", "OFF", nullptr, MIDIMessageType::OFF},
        {"CC", "CC", nullptr, MIDIMessageType::CC},
        {"Note", "NOTE", nullptr, MIDIMessageType::Note},
        {"PitchBend", "PITCH_BEND", "PITCHBEND", MIDIMessageType::PitchBend},
        {"ProgramChange", "PROGRAM", "PROGRAM_CHANGE", MIDIMessageType::ProgramChange},
        {"Aftertouch", "AFTERTOUCH", nullptr, MIDIMessageType::Aftertouch},
        {"ModWheel", "MOD_WHEEL", "MODWHEEL", MIDIMessageType::ModWheel},
        {"NRPN", "NRPN", nullptr, MIDIMessageType::NRPN},
        {"RPN", "RPN", nullptr, MIDIMessageType::RPN},
        {"SysEx", "SYSEX", "SYS_EX", MIDIMessageType::SysEx}};
    for (const auto &entry : kMap) {
        if ((entry.legacy && equalsIgnoreCase(label, entry.legacy)) ||
            (entry.canonical && equalsIgnoreCase(label, entry.canonical)) ||
            (entry.alt && equalsIgnoreCase(label, entry.alt))) {
            type = entry.value;
            return true;
        }
    }
    return false;
}

bool parseSlotType(JsonVariantConst typeField, JsonVariantConst typeNameField,
                   MIDIMessageType &type) {
    auto assignFromIntegral = [&](long candidate) {
        if (candidate < static_cast<long>(MIDIMessageType::OFF) ||
            candidate > static_cast<long>(MIDIMessageType::SysEx)) {
            return false;
        }
        type = static_cast<MIDIMessageType>(candidate);
        return true;
    };

    if (!typeField.isNull()) {
        if (typeField.is<const char *>()) {
            if (parseMIDIType(typeField.as<const char *>(), type)) {
                return true;
            }
        } else if (typeField.is<int>() || typeField.is<long>() || typeField.is<short>() ||
                   typeField.is<signed char>()) {
            if (assignFromIntegral(typeField.as<long>())) {
                return true;
            }
        } else if (typeField.is<unsigned char>() || typeField.is<unsigned short>() ||
                   typeField.is<unsigned int>() || typeField.is<unsigned long>()) {
            unsigned long raw = typeField.as<unsigned long>();
            if (raw <= static_cast<unsigned long>(static_cast<long>(MIDIMessageType::SysEx)) &&
                assignFromIntegral(static_cast<long>(raw))) {
                return true;
            }
        } else if (typeField.is<float>() || typeField.is<double>()) {
            double raw = typeField.as<double>();
            if (std::isfinite(raw)) {
                long candidate = static_cast<long>(raw);
                if (static_cast<double>(candidate) == raw && assignFromIntegral(candidate)) {
                    return true;
                }
            }
        }
    }

    if (!typeNameField.isNull() && typeNameField.is<const char *>()) {
        return parseMIDIType(typeNameField.as<const char *>(), type);
    }
    return false;
}

EnvelopeFollower::FilterType parseFilterType(const char *label,
                                             EnvelopeFollower::FilterType fallback) {
    if (!label)
        return fallback;
    struct Entry {
        const char *name;
        EnvelopeFollower::FilterType value;
    };
    static constexpr Entry kMap[] = {
        {"LINEAR", EnvelopeFollower::LINEAR},
        {"OPPOSITE_LINEAR", EnvelopeFollower::OPPOSITE_LINEAR},
        {"EXPONENTIAL", EnvelopeFollower::EXPONENTIAL},
        {"RANDOM", EnvelopeFollower::RANDOM},
        {"LOWPASS", EnvelopeFollower::LOWPASS},
        {"HIGHPASS", EnvelopeFollower::HIGHPASS},
        {"BANDPASS", EnvelopeFollower::BANDPASS},
    };
    for (const auto &entry : kMap) {
        if (strcmp(label, entry.name) == 0) {
            return entry.value;
        }
    }
    return fallback;
}

EnvelopeFollower::EFMode parseEfMode(const char *label, EnvelopeFollower::EFMode fallback) {
    if (!label) {
        return fallback;
    }
    struct Entry {
        const char *name;
        EnvelopeFollower::EFMode value;
    };
    static constexpr Entry kMap[] = {
        {"PEAK", EnvelopeFollower::EFMode::Peak},
        {"RMS", EnvelopeFollower::EFMode::RMS},
        {"GATE", EnvelopeFollower::EFMode::Gate},
        {"FOLLOWER", EnvelopeFollower::EFMode::Follower},
    };
    for (const auto &entry : kMap) {
        if (strcmp(label, entry.name) == 0) {
            return entry.value;
        }
    }
    return fallback;
}

void parseEfSettings(JsonObject obj, const EfSettings &defaults, EfSettings &out) {
    out = defaults;
    if (obj.isNull()) {
        return;
    }

    if (obj.containsKey("filter_index")) {
        int idx = constrain(obj["filter_index"].as<int>(), 0, 6);
        out.filterType = static_cast<EfSettings::FilterType>(idx);
    } else if (obj.containsKey("filter")) {
        const char *label = obj["filter"].as<const char *>();
        out.filterType = encodeFilterType(parseFilterType(label, decodeFilterType(out.filterType)));
    } else if (obj.containsKey("filter_name")) {
        const char *label = obj["filter_name"].as<const char *>();
        out.filterType = encodeFilterType(parseFilterType(label, decodeFilterType(out.filterType)));
    }

    if (obj.containsKey("frequency")) {
        out.frequency = obj["frequency"].as<float>();
    } else if (obj.containsKey("freq")) {
        out.frequency = obj["freq"].as<float>();
    }

    if (obj.containsKey("q")) {
        out.q = obj["q"].as<float>();
    }

    if (obj.containsKey("oversample")) {
        int oversample = obj["oversample"].as<int>();
        out.oversample = static_cast<uint8_t>(constrain(oversample, 1, 32));
    }

    if (obj.containsKey("smoothing")) {
        out.smoothing = obj["smoothing"].as<float>();
    }

    if (obj.containsKey("baseline")) {
        out.baseline = obj["baseline"].as<float>();
    }

    if (obj.containsKey("gain")) {
        out.gain = obj["gain"].as<float>();
    }

    if (obj.containsKey("mode")) {
        if (obj["mode"].is<const char *>()) {
            out.efMode = static_cast<uint8_t>(parseEfMode(
                obj["mode"].as<const char *>(), static_cast<EnvelopeFollower::EFMode>(out.efMode)));
        } else {
            int raw = obj["mode"].as<int>();
            out.efMode = static_cast<uint8_t>(
                constrain(raw, 0, static_cast<int>(EnvelopeFollower::EFMode::Follower)));
        }
    }

    if (obj.containsKey("auto_baseline")) {
        out.autoBaseline = obj["auto_baseline"].as<bool>() ? 1 : 0;
    } else if (obj.containsKey("autoBaseline")) {
        out.autoBaseline = obj["autoBaseline"].as<bool>() ? 1 : 0;
    }

    if (obj.containsKey("auto_gain")) {
        out.autoGain = obj["auto_gain"].as<bool>() ? 1 : 0;
    } else if (obj.containsKey("autoGain")) {
        out.autoGain = obj["autoGain"].as<bool>() ? 1 : 0;
    }

    if (obj.containsKey("attack_ms")) {
        out.attackMs = static_cast<uint16_t>(obj["attack_ms"].as<int>());
    } else if (obj.containsKey("attackMs")) {
        out.attackMs = static_cast<uint16_t>(obj["attackMs"].as<int>());
    }

    if (obj.containsKey("release_ms")) {
        out.releaseMs = static_cast<uint16_t>(obj["release_ms"].as<int>());
    } else if (obj.containsKey("releaseMs")) {
        out.releaseMs = static_cast<uint16_t>(obj["releaseMs"].as<int>());
    }

    if (obj.containsKey("rms_ms")) {
        out.rmsWindowMs = static_cast<uint16_t>(obj["rms_ms"].as<int>());
    } else if (obj.containsKey("rmsWindowMs")) {
        out.rmsWindowMs = static_cast<uint16_t>(obj["rmsWindowMs"].as<int>());
    }

    if (obj.containsKey("baseline_tau_ms")) {
        out.baselineTauMs = static_cast<uint16_t>(obj["baseline_tau_ms"].as<int>());
    } else if (obj.containsKey("baselineTauMs")) {
        out.baselineTauMs = static_cast<uint16_t>(obj["baselineTauMs"].as<int>());
    }

    if (obj.containsKey("gain_tau_ms")) {
        out.gainTauMs = static_cast<uint16_t>(obj["gain_tau_ms"].as<int>());
    } else if (obj.containsKey("gainTauMs")) {
        out.gainTauMs = static_cast<uint16_t>(obj["gainTauMs"].as<int>());
    }

    if (obj.containsKey("gate_threshold")) {
        out.gateThreshold = static_cast<uint8_t>(obj["gate_threshold"].as<int>());
    } else if (obj.containsKey("gateThreshold")) {
        out.gateThreshold = static_cast<uint8_t>(obj["gateThreshold"].as<int>());
    }

    if (obj.containsKey("gate_hysteresis")) {
        out.gateHysteresis = static_cast<uint8_t>(obj["gate_hysteresis"].as<int>());
    } else if (obj.containsKey("gateHysteresis")) {
        out.gateHysteresis = static_cast<uint8_t>(obj["gateHysteresis"].as<int>());
    }

    if (obj.containsKey("activity_threshold")) {
        out.activityThreshold = static_cast<uint8_t>(obj["activity_threshold"].as<int>());
    } else if (obj.containsKey("activityThreshold")) {
        out.activityThreshold = static_cast<uint8_t>(obj["activityThreshold"].as<int>());
    }

    if (obj.containsKey("gain_target")) {
        out.gainTarget = static_cast<uint8_t>(obj["gain_target"].as<int>());
    } else if (obj.containsKey("gainTarget")) {
        out.gainTarget = static_cast<uint8_t>(obj["gainTarget"].as<int>());
    }

    if (obj.containsKey("index")) {
        int rawIndex = obj["index"].as<int>();
        rawIndex = constrain(rawIndex, -1, static_cast<int>(NUM_ENVELOPES) - 1);
        out.followerIndex = static_cast<int8_t>(rawIndex);
    }
}

bool parseProfileEf(JsonObject obj, ProfileEfSettings &out) {
    if (obj.isNull()) {
        return false;
    }
    if (obj.containsKey("mode")) {
        int raw = obj["mode"].as<int>();
        out.mode = static_cast<uint8_t>(
            constrain(raw, 0, static_cast<int>(EnvelopeFollower::EFMode::Follower)));
    }
    if (obj.containsKey("auto_baseline")) {
        out.autoBaseline = obj["auto_baseline"].as<bool>() ? 1 : 0;
    }
    if (obj.containsKey("auto_gain")) {
        out.autoGain = obj["auto_gain"].as<bool>() ? 1 : 0;
    }
    if (obj.containsKey("attack_ms")) {
        out.attackMs = static_cast<uint16_t>(obj["attack_ms"].as<int>());
    }
    if (obj.containsKey("release_ms")) {
        out.releaseMs = static_cast<uint16_t>(obj["release_ms"].as<int>());
    }
    if (obj.containsKey("rms_ms")) {
        out.rmsWindowMs = static_cast<uint16_t>(obj["rms_ms"].as<int>());
    }
    if (obj.containsKey("baseline_tau_ms")) {
        out.baselineTauMs = static_cast<uint16_t>(obj["baseline_tau_ms"].as<int>());
    }
    if (obj.containsKey("gain_tau_ms")) {
        out.gainTauMs = static_cast<uint16_t>(obj["gain_tau_ms"].as<int>());
    }
    if (obj.containsKey("gate_threshold")) {
        out.gateThreshold = static_cast<uint8_t>(obj["gate_threshold"].as<int>());
    }
    if (obj.containsKey("gate_hysteresis")) {
        out.gateHysteresis = static_cast<uint8_t>(obj["gate_hysteresis"].as<int>());
    }
    if (obj.containsKey("activity_threshold")) {
        out.activityThreshold = static_cast<uint8_t>(obj["activity_threshold"].as<int>());
    }
    if (obj.containsKey("gain_target")) {
        out.gainTarget = static_cast<uint8_t>(obj["gain_target"].as<int>());
    }
    return true;
}

void writeProfileEf(JsonObject obj, const ProfileEfSettings &settings) {
    obj["mode"] = settings.mode;
    obj["auto_baseline"] = settings.autoBaseline != 0;
    obj["auto_gain"] = settings.autoGain != 0;
    obj["attack_ms"] = settings.attackMs;
    obj["release_ms"] = settings.releaseMs;
    obj["rms_ms"] = settings.rmsWindowMs;
    obj["baseline_tau_ms"] = settings.baselineTauMs;
    obj["gain_tau_ms"] = settings.gainTauMs;
    obj["gate_threshold"] = settings.gateThreshold;
    obj["gate_hysteresis"] = settings.gateHysteresis;
    obj["activity_threshold"] = settings.activityThreshold;
    obj["gain_target"] = settings.gainTarget;
}

ARGMethod parseArgMethod(const char *label, ARGMethod fallback) {
    if (!label)
        return fallback;
    struct Entry {
        const char *name;
        ARGMethod value;
    };
    static constexpr Entry kMap[] = {
        {"PLUS", ARGMethod::PLUS}, {"MIN", ARGMethod::MIN},   {"PECK", ARGMethod::PECK},
        {"SHAV", ARGMethod::SHAV}, {"SQAR", ARGMethod::SQAR}, {"BABS", ARGMethod::BABS},
        {"TABS", ARGMethod::TABS}, {"MULT", ARGMethod::MULT}, {"DIVI", ARGMethod::DIVI},
        {"AVG", ARGMethod::AVG},   {"XABS", ARGMethod::XABS}, {"MAXX", ARGMethod::MAXX},
        {"MINN", ARGMethod::MINN}, {"XORR", ARGMethod::XORR}};
    for (const auto &entry : kMap) {
        if (strcmp(label, entry.name) == 0) {
            return entry.value;
        }
    }
    return fallback;
}

EnvelopeFollower::ARG_Method toFollowerArgMethod(ARGMethod method) {
    return static_cast<EnvelopeFollower::ARG_Method>(static_cast<uint8_t>(method));
}

bool parseHexColor(const char *hex, CRGB &color) {
    if (!hex)
        return false;
    if (hex[0] == '#') {
        ++hex;
    }
    if (strlen(hex) != 6)
        return false;
    char *end = nullptr;
    long value = strtol(hex, &end, 16);
    if (!end || *end != '\0')
        return false;
    color.r = static_cast<uint8_t>((value >> 16) & 0xFF);
    color.g = static_cast<uint8_t>((value >> 8) & 0xFF);
    color.b = static_cast<uint8_t>(value & 0xFF);
    return true;
}

void emitBulkError(const char *code, const char *message, uint32_t seq = 0) {
    String out = "{\"type\":\"error\"";
    if (code && code[0] != '\0') {
        out += ",\"code\":\"";
        out += code;
        out += "\"";
    }
    if (seq != 0) {
        out += ",\"seq\":";
        out += seq;
    }
    if (message && message[0] != '\0') {
        out += ",\"message\":\"";
        out += message;
        out += "\"";
    }
    out += "}";
    LOG_PRINTLN(out);
}

bool applyConfigObject(JsonObject config, uint32_t seq) {
    if (config.isNull()) {
        emitBulkError("config_missing", "config object absent", seq);
        return false;
    }

    if (!config.containsKey("slots") || !config["slots"].is<JsonArray>()) {
        emitBulkError("slots_missing", "config.slots missing", seq);
        return false;
    }

    JsonArray slotsJson = config["slots"].as<JsonArray>();
    if (slotsJson.size() != NUM_SLOTS) {
        emitBulkError("slots_size", "unexpected slot count", seq);
        return false;
    }

    EfSettings defaultEfSettings{};
    defaultEfSettings.filterType = encodeFilterType(EnvelopeFollower::LINEAR);
    defaultEfSettings.frequency = 1000.0f;
    defaultEfSettings.q = 0.707f;
    if (config.containsKey("filter") && config["filter"].is<JsonObject>()) {
        JsonObject filterObj = config["filter"].as<JsonObject>();
        parseEfSettings(filterObj, defaultEfSettings, defaultEfSettings);
    }

    SlotARGConfig defaultArg{};
    defaultArg.enabled = configManager.getARGEnable();
    defaultArg.method = static_cast<ARGMethod>(configManager.getARGMethod());
    int storedA = static_cast<int>(configManager.getEnvelopeA());
    if (storedA >= NUM_ENVELOPES) {
        int converted = envelopeIndexFromAnalogPin(storedA);
        storedA = (converted >= 0) ? converted : constrain(storedA, 0, NUM_ENVELOPES - 1);
    }
    int storedB = static_cast<int>(configManager.getEnvelopeB());
    if (storedB >= NUM_ENVELOPES) {
        int converted = envelopeIndexFromAnalogPin(storedB);
        if (converted >= 0) {
            storedB = converted;
        } else {
            storedB = constrain(storedB, 0, NUM_ENVELOPES - 1);
        }
    }
    defaultArg.sourceA = static_cast<uint8_t>(storedA);
    defaultArg.sourceB = static_cast<uint8_t>(storedB);
    defaultArg = sanitizeSlotArg(defaultArg);

    if (config.containsKey("arg") && config["arg"].is<JsonObject>()) {
        JsonObject argObj = config["arg"].as<JsonObject>();
        SlotARGConfig incoming = defaultArg;
        if (argObj.containsKey("enable")) {
            incoming.enabled = argObj["enable"].as<bool>() ? 1 : 0;
        } else if (argObj.containsKey("enabled")) {
            incoming.enabled = argObj["enabled"].as<bool>() ? 1 : 0;
        }
        if (argObj.containsKey("method")) {
            if (argObj["method"].is<const char *>()) {
                ARGMethod parsed =
                    parseArgMethod(argObj["method"].as<const char *>(), incoming.method);
                incoming.method = parsed;
            } else {
                int raw = argObj["method"].as<int>();
                raw = constrain(raw, 0, static_cast<int>(ARGMethod::XORR));
                incoming.method = static_cast<ARGMethod>(raw);
            }
        }
        if (argObj.containsKey("a")) {
            incoming.sourceA =
                static_cast<uint8_t>(constrain(argObj["a"].as<int>(), 0, NUM_ENVELOPES - 1));
        } else if (argObj.containsKey("sourceA")) {
            incoming.sourceA =
                static_cast<uint8_t>(constrain(argObj["sourceA"].as<int>(), 0, NUM_ENVELOPES - 1));
        }
        if (argObj.containsKey("b")) {
            incoming.sourceB =
                static_cast<uint8_t>(constrain(argObj["b"].as<int>(), 0, NUM_ENVELOPES - 1));
        } else if (argObj.containsKey("sourceB")) {
            incoming.sourceB =
                static_cast<uint8_t>(constrain(argObj["sourceB"].as<int>(), 0, NUM_ENVELOPES - 1));
        }

        defaultArg = sanitizeSlotArg(incoming);
    }

    bool anySlotPayloadSpecified = false;
    for (uint8_t i = 0; i < NUM_SLOTS; ++i) {
        JsonObject slotObj = slotsJson[i];
        if (slotObj.isNull()) {
            emitBulkError("slot_null", "slot entry missing", seq);
            return false;
        }

        MIDIMessageType midiType = MIDIMessageType::OFF;
        if (!parseSlotType(slotObj["type"], slotObj["type_name"], midiType)) {
            emitBulkError("slot_type", "unknown slot type", seq);
            return false;
        }

        uint8_t midiChannel = constrain(slotObj["midiChannel"].as<int>(), 1, 16);
        uint8_t data1 = constrain(slotObj["data1"].as<int>(), 0, 127);
        bool active = slotObj["active"].as<bool>();

        MIDISlot &slot = configManager.getSlot(i);
        MIDISlot::EfSettings settings = slot.efSettings;
        settings.followerIndex = slot.getEnvelopeFollowerIndex();

        int rawEfIndex = settings.followerIndex;
        if (slotObj.containsKey("efIndex")) {
            rawEfIndex = slotObj["efIndex"].as<int>();
        } else if (slotObj.containsKey("ef_index")) {
            rawEfIndex = slotObj["ef_index"].as<int>();
        }

        JsonObject efObj =
            slotObj["ef"].is<JsonObject>() ? slotObj["ef"].as<JsonObject>() : JsonObject();
        if (!efObj.isNull()) {
            if (efObj.containsKey("index")) {
                rawEfIndex = efObj["index"].as<int>();
            }
            if (efObj.containsKey("filter_index")) {
                int idx = constrain(efObj["filter_index"].as<int>(), 0, 6);
                settings.filterType = static_cast<MIDISlot::EfSettings::FilterType>(idx);
            } else if (efObj.containsKey("filter_name")) {
                const char *label = efObj["filter_name"].as<const char *>();
                EnvelopeFollower::FilterType parsed =
                    parseFilterType(label, EnvelopeFollower::filterFromEfType(settings.filterType));
                settings.filterType = fromEnvelopeFilter(parsed);
            } else if (efObj.containsKey("filter")) {
                const char *label = efObj["filter"].as<const char *>();
                EnvelopeFollower::FilterType parsed =
                    parseFilterType(label, EnvelopeFollower::filterFromEfType(settings.filterType));
                settings.filterType = fromEnvelopeFilter(parsed);
            }
            if (efObj.containsKey("frequency")) {
                settings.frequency = efObj["frequency"].as<float>();
            }
            if (efObj.containsKey("q")) {
                settings.q = efObj["q"].as<float>();
            }
            if (efObj.containsKey("oversample")) {
                int oversample = efObj["oversample"].as<int>();
                settings.oversample = static_cast<uint8_t>(constrain(oversample, 1, 32));
            }
            if (efObj.containsKey("smoothing")) {
                settings.smoothing = efObj["smoothing"].as<float>();
            }
            if (efObj.containsKey("baseline")) {
                settings.baseline = efObj["baseline"].as<float>();
            }
            if (efObj.containsKey("gain")) {
                settings.gain = efObj["gain"].as<float>();
            }
            if (efObj.containsKey("mode")) {
                if (efObj["mode"].is<const char *>()) {
                    settings.efMode = static_cast<uint8_t>(
                        parseEfMode(efObj["mode"].as<const char *>(),
                                    static_cast<EnvelopeFollower::EFMode>(settings.efMode)));
                } else {
                    int raw = efObj["mode"].as<int>();
                    settings.efMode = static_cast<uint8_t>(
                        constrain(raw, 0, static_cast<int>(EnvelopeFollower::EFMode::Follower)));
                }
            }
            if (efObj.containsKey("auto_baseline")) {
                settings.autoBaseline = efObj["auto_baseline"].as<bool>() ? 1 : 0;
            } else if (efObj.containsKey("autoBaseline")) {
                settings.autoBaseline = efObj["autoBaseline"].as<bool>() ? 1 : 0;
            }
            if (efObj.containsKey("auto_gain")) {
                settings.autoGain = efObj["auto_gain"].as<bool>() ? 1 : 0;
            } else if (efObj.containsKey("autoGain")) {
                settings.autoGain = efObj["autoGain"].as<bool>() ? 1 : 0;
            }
            if (efObj.containsKey("attack_ms")) {
                settings.attackMs = static_cast<uint16_t>(efObj["attack_ms"].as<int>());
            } else if (efObj.containsKey("attackMs")) {
                settings.attackMs = static_cast<uint16_t>(efObj["attackMs"].as<int>());
            }
            if (efObj.containsKey("release_ms")) {
                settings.releaseMs = static_cast<uint16_t>(efObj["release_ms"].as<int>());
            } else if (efObj.containsKey("releaseMs")) {
                settings.releaseMs = static_cast<uint16_t>(efObj["releaseMs"].as<int>());
            }
            if (efObj.containsKey("rms_ms")) {
                settings.rmsWindowMs = static_cast<uint16_t>(efObj["rms_ms"].as<int>());
            } else if (efObj.containsKey("rmsWindowMs")) {
                settings.rmsWindowMs = static_cast<uint16_t>(efObj["rmsWindowMs"].as<int>());
            }
            if (efObj.containsKey("baseline_tau_ms")) {
                settings.baselineTauMs = static_cast<uint16_t>(efObj["baseline_tau_ms"].as<int>());
            } else if (efObj.containsKey("baselineTauMs")) {
                settings.baselineTauMs = static_cast<uint16_t>(efObj["baselineTauMs"].as<int>());
            }
            if (efObj.containsKey("gain_tau_ms")) {
                settings.gainTauMs = static_cast<uint16_t>(efObj["gain_tau_ms"].as<int>());
            } else if (efObj.containsKey("gainTauMs")) {
                settings.gainTauMs = static_cast<uint16_t>(efObj["gainTauMs"].as<int>());
            }
            if (efObj.containsKey("gate_threshold")) {
                settings.gateThreshold = static_cast<uint8_t>(efObj["gate_threshold"].as<int>());
            } else if (efObj.containsKey("gateThreshold")) {
                settings.gateThreshold = static_cast<uint8_t>(efObj["gateThreshold"].as<int>());
            }
            if (efObj.containsKey("gate_hysteresis")) {
                settings.gateHysteresis = static_cast<uint8_t>(efObj["gate_hysteresis"].as<int>());
            } else if (efObj.containsKey("gateHysteresis")) {
                settings.gateHysteresis = static_cast<uint8_t>(efObj["gateHysteresis"].as<int>());
            }
            if (efObj.containsKey("activity_threshold")) {
                settings.activityThreshold =
                    static_cast<uint8_t>(efObj["activity_threshold"].as<int>());
            } else if (efObj.containsKey("activityThreshold")) {
                settings.activityThreshold =
                    static_cast<uint8_t>(efObj["activityThreshold"].as<int>());
            }
            if (efObj.containsKey("gain_target")) {
                settings.gainTarget = static_cast<uint8_t>(efObj["gain_target"].as<int>());
            } else if (efObj.containsKey("gainTarget")) {
                settings.gainTarget = static_cast<uint8_t>(efObj["gainTarget"].as<int>());
            }
        }

        if (rawEfIndex >= 0 && rawEfIndex < static_cast<int>(envelopeFollowers.size())) {
            settings.followerIndex = static_cast<int8_t>(rawEfIndex);
        } else {
            settings.followerIndex = -1;
        }

        slot.type = midiType;
        slot.midiChannel = midiChannel;
        slot.data1 = data1;
        slot.efSettings = settings;
        slot.setEnvelopeFollowerIndex(settings.followerIndex);
        slot.active = active;
        slot.arg = defaultArg;
        if (slotObj.containsKey("arg") && slotObj["arg"].is<JsonObject>()) {
            JsonObject slotArgObj = slotObj["arg"].as<JsonObject>();
            SlotARGConfig slotArgConfig = slot.arg;
            if (slotArgObj.containsKey("enable")) {
                slotArgConfig.enabled = slotArgObj["enable"].as<bool>() ? 1 : 0;
            } else if (slotArgObj.containsKey("enabled")) {
                slotArgConfig.enabled = slotArgObj["enabled"].as<bool>() ? 1 : 0;
            }
            if (slotArgObj.containsKey("method")) {
                if (slotArgObj["method"].is<const char *>()) {
                    ARGMethod parsed = parseArgMethod(slotArgObj["method"].as<const char *>(),
                                                      slotArgConfig.method);
                    slotArgConfig.method = parsed;
                } else {
                    int raw = slotArgObj["method"].as<int>();
                    raw = constrain(raw, 0, static_cast<int>(ARGMethod::XORR));
                    slotArgConfig.method = static_cast<ARGMethod>(raw);
                }
            }
            if (slotArgObj.containsKey("a")) {
                slotArgConfig.sourceA = static_cast<uint8_t>(
                    constrain(slotArgObj["a"].as<int>(), 0, NUM_ENVELOPES - 1));
            } else if (slotArgObj.containsKey("sourceA")) {
                slotArgConfig.sourceA = static_cast<uint8_t>(
                    constrain(slotArgObj["sourceA"].as<int>(), 0, NUM_ENVELOPES - 1));
            }
            if (slotArgObj.containsKey("b")) {
                slotArgConfig.sourceB = static_cast<uint8_t>(
                    constrain(slotArgObj["b"].as<int>(), 0, NUM_ENVELOPES - 1));
            } else if (slotArgObj.containsKey("sourceB")) {
                slotArgConfig.sourceB = static_cast<uint8_t>(
                    constrain(slotArgObj["sourceB"].as<int>(), 0, NUM_ENVELOPES - 1));
            }
            slot.arg = sanitizeSlotArg(slotArgConfig);
        }
        if (slot.type == MIDIMessageType::SysEx) {
            String templateError;
            if (!parseSysExTemplateField(slotObj["sysexTemplate"], slot, templateError)) {
                emitBulkError("sysex_template", templateError.c_str(), seq);
                return false;
            }
        } else {
            clearSysExTemplate(slot);
        }
        SlotARGConfig slotArg = defaultArg;
        if (slotObj.containsKey("arg") && slotObj["arg"].is<JsonObject>()) {
            JsonObject argObj = slotObj["arg"].as<JsonObject>();
            if (argObj.containsKey("enable")) {
                slotArg.enabled = argObj["enable"].as<bool>() ? 1 : 0;
            } else if (argObj.containsKey("enabled")) {
                slotArg.enabled = argObj["enabled"].as<bool>() ? 1 : 0;
            }
            if (argObj.containsKey("method")) {
                if (argObj["method"].is<const char *>()) {
                    slotArg.method =
                        parseArgMethod(argObj["method"].as<const char *>(), slotArg.method);
                } else {
                    slotArg.method = static_cast<ARGMethod>(constrain(
                        argObj["method"].as<int>(), 0, static_cast<int>(ARGMethod::XORR)));
                }
            }
            if (argObj.containsKey("a")) {
                int rawA = argObj["a"].as<int>();
                int idxA = envelopeIndexFromAnalogPin(rawA);
                if (idxA < 0)
                    idxA = constrain(rawA, 0, NUM_ENVELOPES - 1);
                slotArg.sourceA = static_cast<uint8_t>(idxA);
            }
            if (argObj.containsKey("b")) {
                int rawB = argObj["b"].as<int>();
                int idxB = envelopeIndexFromAnalogPin(rawB);
                if (idxB < 0)
                    idxB = constrain(rawB, 0, NUM_ENVELOPES - 1);
                slotArg.sourceB = static_cast<uint8_t>(idxB);
            }
            slotArg = sanitizeSlotArg(slotArg);
        }
        slot.arg = slotArg;
        configManager.saveSlot(i, slot);

        if (slotObj.containsKey("ef_payload") && slotObj["ef_payload"].is<JsonObject>()) {
            JsonObject efPayload = slotObj["ef_payload"].as<JsonObject>();
            SlotEnvelopePayload payload = configManager.getSlotEnvelopePayload(i);

            if (efPayload.containsKey("type_index")) {
                payload.filterType = static_cast<uint8_t>(efPayload["type_index"].as<int>());
            } else if (efPayload.containsKey("type")) {
                const char *label = efPayload["type"].as<const char *>();
                EnvelopeFollower::FilterType mapped = parseFilterType(
                    label, static_cast<EnvelopeFollower::FilterType>(payload.filterType));
                payload.filterType = static_cast<uint8_t>(mapped);
            }
            if (efPayload.containsKey("freq")) {
                payload.frequency = efPayload["freq"].as<float>();
            }
            if (efPayload.containsKey("q")) {
                payload.q = efPayload["q"].as<float>();
            }

            configManager.setSlotEnvelopePayload(i, payload);
            anySlotPayloadSpecified = true;
        }

        configManager.setPotChannel(i, midiChannel);
        configManager.setPotCCNumber(i, data1);
        potentiometerManager.setChannel(i, midiChannel);
        potentiometerManager.setCCNumber(i, data1);
        if (static_cast<size_t>(i) < potChannels.size()) {
            potChannels[i] = midiChannel;
        }
    }

    configManager.saveConfiguration();

    if (config.containsKey("efSlots") && config["efSlots"].is<JsonArray>()) {
        JsonArray efSlots = config["efSlots"].as<JsonArray>();
        potToEnvelopeMap.clear();

        // Reset all slot->follower wiring first; efSlots repopulates the map below.
        for (uint8_t slotIndex = 0; slotIndex < NUM_SLOTS; ++slotIndex) {
            MIDISlot &slot = configManager.getSlot(slotIndex);
            slot.setEnvelopeFollowerIndex(-1);
        }

        auto assignFollowerToSlot = [&](int followerIndex, int slotIndex) {
            if (followerIndex < 0 || followerIndex >= static_cast<int>(envelopeFollowers.size())) {
                return;
            }
            if (slotIndex < 0 || slotIndex >= NUM_POTS) {
                return;
            }
            MIDISlot &slot = configManager.getSlot(static_cast<uint8_t>(slotIndex));
            slot.setEnvelopeFollowerIndex(static_cast<int8_t>(followerIndex));
            potToEnvelopeMap[slotIndex] = slot.efSettings;
        };

        for (uint8_t i = 0; i < efSlots.size(); ++i) {
            JsonObject mapping = efSlots[i];
            if (mapping.isNull()) {
                continue;
            }

            int followerIndex = static_cast<int>(i);
            if (mapping.containsKey("index")) {
                followerIndex = mapping["index"].as<int>();
            }
            if (followerIndex < 0 || followerIndex >= static_cast<int>(envelopeFollowers.size())) {
                continue;
            }

            if (mapping.containsKey("slots") && mapping["slots"].is<JsonArray>()) {
                JsonArray targets = mapping["slots"].as<JsonArray>();
                for (JsonVariant value : targets) {
                    assignFollowerToSlot(followerIndex, value.as<int>());
                }
            } else if (mapping.containsKey("slot")) {
                assignFollowerToSlot(followerIndex, mapping["slot"].as<int>());
            }
        }

        std::array<bool, NUM_ENVELOPES> followerConfigured{};
        followerConfigured.fill(false);
        for (const auto &entry : potToEnvelopeMap) {
            const int followerIndex = entry.second.followerIndex;
            if (followerIndex < 0 || followerIndex >= static_cast<int>(envelopeFollowers.size())) {
                continue;
            }
            envelopeFollowers[followerIndex].setModulationTarget(
                potentiometerManager.getCCNumber(entry.first));
            if (!followerConfigured[followerIndex]) {
                applyEfSettingsToFollower(envelopeFollowers[followerIndex], entry.second,
                                          static_cast<uint8_t>(followerIndex));
                followerConfigured[followerIndex] = true;
            }
        }
        configManager.saveEnvelopeSettings(potToEnvelopeMap, envelopeFollowers);
    }

    if (config.containsKey("filter") && config["filter"].is<JsonObject>()) {
        JsonObject filterObj = config["filter"].as<JsonObject>();
        EnvelopeFollower::FilterType current = envelopeFollowers.empty()
                                                   ? EnvelopeFollower::LINEAR
                                                   : envelopeFollowers.front().getFilterType();
        EnvelopeFollower::FilterType filterType =
            parseFilterType(filterObj["type"].as<const char *>(), current);
        float freq = constrain(filterObj["freq"].as<float>(), 20.0f, 5000.0f);
        float q = constrain(filterObj["q"].as<float>(), 0.5f, 4.0f);

        SlotEnvelopePayload tailPayload{};
        tailPayload.filterType = static_cast<uint8_t>(filterType);
        tailPayload.frequency = freq;
        tailPayload.q = q;
        SlotEnvelopePayload sanitizedTail = configManager.persistFilterTail(tailPayload);
        filterType = static_cast<EnvelopeFollower::FilterType>(sanitizedTail.filterType);
        freq = sanitizedTail.frequency;
        q = sanitizedTail.q;

        for (auto &ef : envelopeFollowers) {
            ef.setFilterType(filterType);
            ef.configureFilter(freq, q);
        }

        if (!anySlotPayloadSpecified) {
            for (uint8_t i = 0; i < NUM_SLOTS; ++i) {
                SlotEnvelopePayload payload = configManager.getSlotEnvelopePayload(i);
                payload.filterType = static_cast<uint8_t>(filterType);
                payload.frequency = freq;
                payload.q = q;
                configManager.setSlotEnvelopePayload(i, payload);
            }
        }
    }

    envelopeFollowMode = defaultArg.enabled != 0;
    configManager.setARGEnable(defaultArg.enabled);
    configManager.setARGMethod(static_cast<uint8_t>(defaultArg.method));
    configManager.setEnvelopePair(defaultArg.sourceA, defaultArg.sourceB);
    potentiometerManager.setArgEnvelopePair(defaultArg.sourceA, defaultArg.sourceB);

    EnvelopeFollower::ARG_Method followerMethod = toFollowerArgMethod(defaultArg.method);
    for (auto &ef : envelopeFollowers) {
        ef.setARGMethod(followerMethod);
        ef.setMode(envelopeFollowMode ? EnvelopeFollower::ARG : EnvelopeFollower::SEF);
    }

    if (config.containsKey("envelopeMode")) {
        updateEnvelopeModeLabel(config["envelopeMode"].as<const char *>());
    }

    if (config.containsKey("led") && config["led"].is<JsonObject>()) {
        JsonObject ledObj = config["led"].as<JsonObject>();
        uint8_t brightness = constrain(ledObj["brightness"].as<int>(), 0, 255);
        CRGB color = ledManager.getColor();
        const char *hex = ledObj["color"].as<const char *>();
        if (hex) {
            CRGB parsed;
            if (parseHexColor(hex, parsed)) {
                color = parsed;
            }
        }
        ledManager.setBrightness(brightness);
        ledManager.setColor(color);
        configManager.saveLEDSettings(brightness, color);
        if (ledObj.containsKey("mode")) {
            const char *modeStr = ledObj["mode"].as<const char *>();
            LedMode newMode = ledModeFromString(modeStr, configManager.getLedMode());
            configManager.setLedMode(newMode);
            ledAnimator.setMode(newMode);
        }
    }

    return true;
}

namespace {
struct ParsedCommand {
    explicit ParsedCommand(const String &source)
        : command(source), data(source.c_str()), length(source.length()),
          nameLen(measureNameLength(data, length)) {}

    const String &fullCommand() const { return command; }
    const char *c_str() const { return data; }
    size_t size() const { return length; }
    size_t nameLength() const { return nameLen; }
    const char *payload() const { return data + nameLen; }
    size_t payloadLength() const { return (length > nameLen) ? (length - nameLen) : 0U; }

    int compareName(const char *target) const {
        size_t targetLen = std::strlen(target);
        size_t cmpLen = std::min(nameLen, targetLen);
        int cmp = std::memcmp(data, target, cmpLen);
        if (cmp != 0) {
            return cmp;
        }
        if (nameLen < targetLen) {
            return -1;
        }
        if (nameLen > targetLen) {
            return 1;
        }
        return 0;
    }

  private:
    static size_t measureNameLength(const char *text, size_t capacity) {
        size_t index = 0;
        while (index < capacity) {
            char c = text[index];
            if (c == ',' || std::isspace(static_cast<unsigned char>(c))) {
                break;
            }
            ++index;
        }
        return index;
    }

    const String &command;
    const char *data;
    size_t length;
    size_t nameLen;
};

struct CommandHandler {
    const char *name;
    void (*handler)(const ParsedCommand &cmd);
};

void handleGetAllCommand(const ParsedCommand &cmd);
void handleGetArgMethodCommand(const ParsedCommand &cmd);
void handleGetBrownoutsCommand(const ParsedCommand &cmd);
void handleGetConfigCommand(const ParsedCommand &cmd);
void handleGetEfCommand(const ParsedCommand &cmd);
void handleGetLedCommand(const ParsedCommand &cmd);
void handleGetManifestCommand(const ParsedCommand &cmd);
void handleGetProfileCommand(const ParsedCommand &cmd);
void handleGetSchemaCommand(const ParsedCommand &cmd);
void handleHelloCommand(const ParsedCommand &cmd);
void handleLoadProfileCommand(const ParsedCommand &cmd);
void handleRecallMacroSlotCommand(const ParsedCommand &cmd);
void handleResetProfileCommand(const ParsedCommand &cmd);
void handleSaveProfileCommand(const ParsedCommand &cmd);
void handleSaveMacroSlotCommand(const ParsedCommand &cmd);
void handleSetAllCommand(const ParsedCommand &cmd);
void handleSetArgMethodCommand(const ParsedCommand &cmd);
void handleSetEfCommand(const ParsedCommand &cmd);
void handleSetLedCommand(const ParsedCommand &cmd);
void handleSetPotCommand(const ParsedCommand &cmd);
void handleSetProfileCommand(const ParsedCommand &cmd);
void handleSetSlotValueCommand(const ParsedCommand &cmd);

// Keep this table lexicographically sorted; `findCommandHandler()` does a binary search.
const CommandHandler kCommandHandlers[] = {
    {"GET_ALL", handleGetAllCommand},
    {"GET_ARGMETHOD", handleGetArgMethodCommand},
    {"GET_BROWNOUTS", handleGetBrownoutsCommand},
    {"GET_CONFIG", handleGetConfigCommand},
    {"GET_EF", handleGetEfCommand},
    {"GET_LED", handleGetLedCommand},
    {"GET_MANIFEST", handleGetManifestCommand},
    {"GET_PROFILE", handleGetProfileCommand},
    {"GET_SCHEMA", handleGetSchemaCommand},
    {"HELLO", handleHelloCommand},
    {"LOAD_PROFILE", handleLoadProfileCommand},
    {"RECALL_MACRO_SLOT", handleRecallMacroSlotCommand},
    {"RESET_PROFILE", handleResetProfileCommand},
    {"SAVE_MACRO_SLOT", handleSaveMacroSlotCommand},
    {"SAVE_PROFILE", handleSaveProfileCommand},
    {"SET_ALL", handleSetAllCommand},
    {"SET_ARGMETHOD", handleSetArgMethodCommand},
    {"SET_EF", handleSetEfCommand},
    {"SET_LED", handleSetLedCommand},
    {"SET_POT", handleSetPotCommand},
    {"SET_PROFILE", handleSetProfileCommand},
    {"SET_SLOT_VALUE", handleSetSlotValueCommand},
};

constexpr size_t kCommandHandlerCount = sizeof(kCommandHandlers) / sizeof(kCommandHandlers[0]);

const CommandHandler *findCommandHandler(const ParsedCommand &cmd) {
    size_t low = 0;
    size_t high = kCommandHandlerCount;
    while (low < high) {
        size_t mid = low + (high - low) / 2;
        int comparison = cmd.compareName(kCommandHandlers[mid].name);
        if (comparison == 0) {
            return &kCommandHandlers[mid];
        }
        if (comparison < 0) {
            high = mid;
        } else {
            low = mid + 1;
        }
    }
    return nullptr;
}

void logUnknownCommand(const String &command) {
    LOG_PRINTLN("Unknown command: " + command);
    LOG_PRINT("Available commands: ");
    for (size_t i = 0; i < kCommandHandlerCount; ++i) {
        LOG_PRINT(kCommandHandlers[i].name);
        if (i + 1 < kCommandHandlerCount) {
            LOG_PRINT(", ");
        }
    }
    LOG_PRINTLN("");
}

bool dispatchCommand(const String &command) {
    ParsedCommand parsed(command);
    if (const CommandHandler *handler = findCommandHandler(parsed)) {
        handler->handler(parsed);
        return true;
    }
    if (configManager.handleCommand(command)) {
        return true;
    }
    logUnknownCommand(command);
    return false;
}
} // namespace

void processCommandQueue() {
    // Keep one persistent String scratch buffer to avoid per-command heap churn.
    static String command;
    static bool commandInitialized = false;
    if (!commandInitialized) {
        command.reserve(SERIAL_BUFFER_SIZE - 1);
        commandInitialized = true;
    }

    char line[SERIAL_BUFFER_SIZE];
    while (dequeueSerialCommand(line, sizeof(line))) {
        command = line;

        command.trim();

        // JSON scene commands intentionally short-circuit before legacy CSV-style command parsing.
        if (handleSceneJsonCommand(command)) {
            continue;
        }

        dispatchCommand(command);
    }
}

namespace {
void handleGetAllCommand(const ParsedCommand &cmd) {
    (void)cmd;
#ifdef SERIAL_LOGGING
    // Send all pot settings
    LOG_PRINT("POTS:");
    for (int i = 0; i < NUM_POTS; i++) {
        int envelopeValue = -1;
        auto it = potToEnvelopeMap.find(i);
        if (it != potToEnvelopeMap.end()) {
            envelopeValue = it->second.followerIndex;
        }
        LOG_PRINT(configManager.getPotCCNumber(i));
        LOG_PRINT(",");
        LOG_PRINT(configManager.getPotChannel(i));
        LOG_PRINT(",");
        LOG_PRINT(envelopeValue);
        LOG_PRINT(";");
    }

    // Send LED settings
    CRGB ledColor = ledManager.getColor();
    LOG_PRINT(" LED:");
    LOG_PRINT(ledManager.getBrightness());
    LOG_PRINT(",");
    LOG_PRINT(ledColor.r);
    LOG_PRINT(",");
    LOG_PRINT(ledColor.g);
    LOG_PRINT(",");
    LOG_PRINTLN(ledColor.b);
#endif
}

void handleGetArgMethodCommand(const ParsedCommand &cmd) {
    (void)cmd;
    LOG_PRINTLN(configManager.getARGMethod());
}

void handleGetBrownoutsCommand(const ParsedCommand &cmd) {
    (void)cmd;
    LOG_PRINTLN(g_brownoutCount);
}

void handleGetConfigCommand(const ParsedCommand &cmd) {
    (void)cmd;
    StaticJsonDocument<8192> doc;

    doc["fw_version"] = FW_VERSION_STR;
    doc["schema_version"] = CONFIG_VERSION;

    JsonArray pots = doc.createNestedArray("pots");
    for (uint8_t i = 0; i < configManager.getNumPots(); ++i) {
        JsonObject pot = pots.createNestedObject();
        pot["index"] = i;
        pot["channel"] = configManager.getPotChannel(i);
        pot["cc"] = configManager.getPotCCNumber(i);
    }

    JsonArray slots = doc.createNestedArray("slots");
    const auto &slotDefs = configManager.getSlots();
    for (uint8_t i = 0; i < NUM_SLOTS; ++i) {
        const MIDISlot &slot = slotDefs[i];
        JsonObject slotObj = slots.createNestedObject();
        slotObj["index"] = i;
        slotObj["type"] = static_cast<uint8_t>(slot.type);
        slotObj["type_name"] = midiMessageTypeName(slot.type);
        slotObj["channel"] = slot.midiChannel;
        slotObj["data1"] = slot.data1;
        slotObj["ef_index"] = slot.ef.followerIndex;
        JsonObject ef = slotObj.createNestedObject("ef");
        ef["index"] = slot.ef.followerIndex;
        ef["filter_index"] = static_cast<uint8_t>(slot.efSettings.filterType);
        ef["filter_name"] = efFilterLabel(slot.efSettings.filterType);
        ef["frequency"] = slot.efSettings.frequency;
        ef["q"] = slot.efSettings.q;
        ef["oversample"] = slot.efSettings.oversample;
        ef["smoothing"] = slot.efSettings.smoothing;
        ef["baseline"] = slot.efSettings.baseline;
        ef["gain"] = slot.efSettings.gain;
        ef["mode"] = slot.efSettings.efMode;
        ef["auto_baseline"] = slot.efSettings.autoBaseline != 0;
        ef["auto_gain"] = slot.efSettings.autoGain != 0;
        ef["attack_ms"] = slot.efSettings.attackMs;
        ef["release_ms"] = slot.efSettings.releaseMs;
        ef["rms_ms"] = slot.efSettings.rmsWindowMs;
        ef["baseline_tau_ms"] = slot.efSettings.baselineTauMs;
        ef["gain_tau_ms"] = slot.efSettings.gainTauMs;
        ef["gate_threshold"] = slot.efSettings.gateThreshold;
        ef["gate_hysteresis"] = slot.efSettings.gateHysteresis;
        ef["activity_threshold"] = slot.efSettings.activityThreshold;
        ef["gain_target"] = slot.efSettings.gainTarget;
        slotObj["active"] = slot.active;
        slotObj["arp_note"] = slot.arpNote;
        slotObj["sysexTemplate"] = formatSysExTemplate(slot);
        SlotEnvelopePayload payload = configManager.getSlotEnvelopePayload(i);
        JsonObject efPayload = slotObj.createNestedObject("ef_payload");
        efPayload["type"] = payload.filterType;
        efPayload["type_name"] =
            envelopeFilterName(static_cast<EnvelopeFollower::FilterType>(payload.filterType));
        efPayload["freq"] = payload.frequency;
        efPayload["q"] = payload.q;
        SlotARGConfig arg = sanitizeSlotArg(slot.arg);
        JsonObject argObj = slotObj.createNestedObject("arg");
        argObj["enabled"] = arg.enabled != 0;
        argObj["method"] = static_cast<uint8_t>(arg.method);
        argObj["method_name"] = argMethodName(static_cast<uint8_t>(arg.method));
        argObj["sourceA"] = arg.sourceA;
        argObj["sourceB"] = arg.sourceB;
    }

    JsonArray efSlots = doc.createNestedArray("efSlots");
    for (uint8_t followerIndex = 0; followerIndex < NUM_ENVELOPES; ++followerIndex) {
        JsonObject mapping = efSlots.createNestedObject();
        mapping["index"] = followerIndex;
        JsonArray targets = mapping.createNestedArray("slots");
        for (uint8_t slotIndex = 0; slotIndex < NUM_POTS; ++slotIndex) {
            auto it = potToEnvelopeMap.find(slotIndex);
            if (it == potToEnvelopeMap.end()) {
                continue;
            }
            if (it->second.followerIndex != static_cast<int8_t>(followerIndex)) {
                continue;
            }
            targets.add(slotIndex);
        }
        if (targets.size() == 1) {
            mapping["slot"] = targets[0].as<uint8_t>();
        }
    }

    JsonObject env = doc.createNestedObject("envelopes");
    JsonArray routing = env.createNestedArray("routing");
    for (uint8_t i = 0; i < NUM_POTS; ++i) {
        int mapping = -1;
        auto it = potToEnvelopeMap.find(i);
        if (it != potToEnvelopeMap.end()) {
            mapping = it->second.followerIndex;
        }
        routing.add(mapping);
    }

    JsonArray followers = env.createNestedArray("followers");
    for (size_t i = 0; i < envelopeFollowers.size(); ++i) {
        JsonObject follower = followers.createNestedObject();
        follower["index"] = static_cast<uint8_t>(i);
        follower["active"] = envelopeFollowers[i].getActiveState();
        follower["filter"] = envelopeFilterName(envelopeFollowers[i].getFilterType());
        follower["baseline"] = envelopeConfig.baselines[i];
        follower["oversample"] = envelopeFollowers[i].getOversampleCount();
        follower["smoothing"] = envelopeFollowers[i].getSmoothingAlpha();
    }

    uint8_t storedMode = configManager.getMode();
    env["mode"] = storedMode;
    env["mode_name"] = envelopeModeName(storedMode);

    uint8_t storedMethod = configManager.getARGMethod();
    env["arg_method"] = storedMethod;
    env["arg_method_name"] = argMethodName(storedMethod);
    env["arg_enable"] = configManager.getARGEnable();

    JsonObject argPair = env.createNestedObject("arg_pair");
    argPair["a"] = configManager.getEnvelopeA();
    argPair["b"] = configManager.getEnvelopeB();

    float freq = 0.0f;
    float q = 0.0f;
    EEPROM.get(EEPROM_FILTER_FREQ, freq);
    EEPROM.get(EEPROM_FILTER_Q, q);
    JsonObject filter = env.createNestedObject("filter");
    filter["frequency"] = freq;
    filter["q"] = q;

    JsonObject led = doc.createNestedObject("led");
    led["brightness"] = ledManager.getBrightness();
    CRGB color = ledManager.getColor();
    JsonObject colorObj = led.createNestedObject("rgb");
    colorObj["r"] = color.r;
    colorObj["g"] = color.g;
    colorObj["b"] = color.b;
    char hex[8];
    snprintf(hex, sizeof(hex), "#%02X%02X%02X", color.r, color.g, color.b);
    led["hex"] = hex;
    led["mode"] = ledModeToString(configManager.getLedMode());

    String payload;
    serializeJson(doc, payload);
    LOG_PRINTLN(payload);
}

void handleGetEfCommand(const ParsedCommand &cmd) {
    const String &command = cmd.fullCommand();
    int potIndex = command.substring(7).toInt();
    if (potIndex >= 0 && potIndex < NUM_POTS) {
        int env = -1;
        auto it = potToEnvelopeMap.find(potIndex);
        if (it != potToEnvelopeMap.end()) {
            env = it->second.followerIndex;
        }
#ifdef SERIAL_LOGGING
        LOG_PRINTLN(env);
#else
        (void)env;
#endif
    } else {
        LOG_PRINTLN("ERR");
    }
}

void handleGetLedCommand(const ParsedCommand &cmd) {
    (void)cmd;
#ifdef SERIAL_LOGGING
    CRGB c = ledManager.getColor();
    LOG_PRINT(ledManager.getBrightness());
    LOG_PRINT(",");
    LOG_PRINT(c.r);
    LOG_PRINT(",");
    LOG_PRINT(c.g);
    LOG_PRINT(",");
    LOG_PRINTLN(c.b);
#endif
}

void handleGetManifestCommand(const ParsedCommand &cmd) {
    (void)cmd;
    // Manifest is the host's capability contract for this session
    // (schema/version/counts/resources).
    StaticJsonDocument<256> doc;
    writeManifestFields(doc.to<JsonObject>());

    String payload;
    serializeJson(doc, payload);
    LOG_PRINTLN(payload);
}

void handleGetProfileCommand(const ParsedCommand &cmd) {
    const String &command = cmd.fullCommand();
    // If a profile slot has never been persisted, fall back to a live runtime snapshot so hosts
    // still receive a complete payload.
    int comma = command.indexOf(',');
    uint8_t id = g_activeProfile;
    if (comma >= 0) {
        id = static_cast<uint8_t>(command.substring(comma + 1).toInt());
    }
    if (id >= NUM_PROFILES) {
        LOG_PRINTLN("ERR");
        return;
    }
    ProfileData profile{};
    bool stored = configManager.loadProfileSettings(id, profile);
    if (!stored) {
        profile = captureProfileSnapshot();
    }

    StaticJsonDocument<12288> doc;
    doc["profile"] = id;
    doc["stored"] = stored;

    JsonObject arp = doc.createNestedObject("arp");
    arp["length_ticks"] = profile.arp.lengthTicks;
    arp["shape"] = profile.arp.shape;
    arp["swing_percent"] = profile.arp.swingPercent;
    arp["gate_percent"] = profile.arp.gatePercent;
    arp["octave_range"] = profile.arp.octaveRange;

    JsonObject led = doc.createNestedObject("led");
    led["brightness"] = profile.led.brightness;
    JsonObject rgb = led.createNestedObject("rgb");
    rgb["r"] = profile.led.r;
    rgb["g"] = profile.led.g;
    rgb["b"] = profile.led.b;

    JsonArray lfos = doc.createNestedArray("lfos");
    for (uint8_t i = 0; i < PROFILE_LFO_COUNT; ++i) {
        JsonObject lfo = lfos.createNestedObject();
        lfo["index"] = i;
        lfo["shape"] = profile.lfos[i].shape;
        lfo["frequency_hz"] = profile.lfos[i].frequencyHz;
        lfo["depth"] = profile.lfos[i].depth;
        lfo["bipolar"] = profile.lfos[i].bipolar != 0;
        lfo["sync"] = profile.lfos[i].syncEnabled != 0;
        lfo["sync_ratio"] = profile.lfos[i].syncRatio;
    }

    JsonArray routes = doc.createNestedArray("routes");
    for (uint8_t i = 0; i < profile.routeCount && i < PROFILE_MAX_ROUTES; ++i) {
        JsonObject route = routes.createNestedObject();
        route["index"] = i;
        route["type"] = profile.routes[i].type;
        route["lfo"] = profile.routes[i].lfoIndex;
        route["depth"] = profile.routes[i].depth;
        route["target"] = profile.routes[i].target;
        route["channel"] = profile.routes[i].channel;
        route["cc_msb"] = profile.routes[i].ccMsb;
        route["cc_lsb"] = profile.routes[i].ccLsb;
    }

    JsonArray slots = doc.createNestedArray("slots");
    for (uint8_t i = 0; i < NUM_SLOTS; ++i) {
        JsonObject slot = slots.createNestedObject();
        slot["index"] = i;
        slot["channel"] = profile.slots[i].midiChannel;
        JsonObject ef = slot.createNestedObject("ef");
        writeProfileEf(ef, profile.slots[i].ef);
    }

    String payload;
    serializeJson(doc, payload);
    LOG_PRINTLN(payload);
}

void handleGetSchemaCommand(const ParsedCommand &cmd) {
    (void)cmd;
    LOG_PRINTLN(ConfigManager::makeSchema());
}

void handleHelloCommand(const ParsedCommand &cmd) {
    (void)cmd;
    // HELLO is both identity ping and telemetry opt-in for WebSerial clients.
    webSerialStreaming = true;
    LOG_PRINTLN("{\"hello\":\"mn42\"}");
}

void handleLoadProfileCommand(const ParsedCommand &cmd) {
    const String &command = cmd.fullCommand();
    int comma = command.indexOf(',');
    int id = comma >= 0 ? command.substring(comma + 1).toInt() : static_cast<int>(g_activeProfile);
    StaticJsonDocument<160> response;
    response["profile"] = id;
    if (id < 0 || id >= NUM_PROFILES || !loadProfileSlot(static_cast<uint8_t>(id))) {
        response["profile_loaded"] = false;
        response["error"] = "Profile load failed";
        sendJsonResponse(response);
        return;
    }
    response["profile_loaded"] = true;
    sendJsonResponse(response);
}

void handleRecallMacroSlotCommand(const ParsedCommand &cmd) {
    (void)cmd;
    SceneStorage::ConfigState snapshot{};
    bool available = SceneStorage::macroSnapshotAvailable();
    bool recalled = false;
    if (available && SceneStorage::loadMacroSnapshot(snapshot)) {
        SceneStorage::applyConfigState(snapshot, true);
        recalled = true;
    }
    StaticJsonDocument<128> response;
    response["macro_recalled"] = recalled;
    response["macro_available"] = SceneStorage::macroSnapshotAvailable();
    if (!recalled) {
        response["error"] = available ? "Macro recall failed" : "No macro stored";
    }
    sendJsonResponse(response);
}

void handleSaveMacroSlotCommand(const ParsedCommand &cmd) {
    (void)cmd;
    SceneStorage::ConfigState snapshot = SceneStorage::captureConfigState();
    bool saved = SceneStorage::saveMacroSnapshot(snapshot);
    StaticJsonDocument<128> response;
    response["macro_saved"] = saved;
    response["macro_available"] = SceneStorage::macroSnapshotAvailable();
    if (!saved) {
        response["error"] = "Macro snapshot save failed";
    }
    sendJsonResponse(response);
}

void handleResetProfileCommand(const ParsedCommand &cmd) {
    const String &command = cmd.fullCommand();
    int comma = command.indexOf(',');
    int id = comma >= 0 ? command.substring(comma + 1).toInt() : static_cast<int>(g_activeProfile);
    StaticJsonDocument<160> response;
    response["profile"] = id;
    if (id < 0 || id >= NUM_PROFILES || !resetProfileSlot(static_cast<uint8_t>(id))) {
        response["profile_reset"] = false;
        response["error"] = "Profile reset failed";
        sendJsonResponse(response);
        return;
    }
    response["profile_reset"] = true;
    sendJsonResponse(response);
}

void handleSaveProfileCommand(const ParsedCommand &cmd) {
    const String &command = cmd.fullCommand();
    int comma = command.indexOf(',');
    int id = comma >= 0 ? command.substring(comma + 1).toInt() : static_cast<int>(g_activeProfile);
    StaticJsonDocument<160> response;
    response["profile"] = id;
    if (id < 0 || id >= NUM_PROFILES || !saveCurrentProfileSlot(static_cast<uint8_t>(id))) {
        response["profile_saved"] = false;
        response["error"] = "Profile save failed";
        sendJsonResponse(response);
        return;
    }
    response["profile_saved"] = true;
    sendJsonResponse(response);
}

void handleSetAllCommand(const ParsedCommand &cmd) {
    const String &command = cmd.fullCommand();
    String chunk = command.substring(8);
    chunk.trim();
    if (chunk.length() == 0) {
        return;
    }

    String ingestError;
    if (!bulkConfigAssembler.ingestChunk(chunk, ingestError)) {
        uint32_t hint = bulkConfigAssembler.sequenceHint();
        if (ingestError == "overflow") {
            emitBulkError("overflow", "config payload too large", hint);
        } else if (ingestError == "orphan") {
            emitBulkError("orphan", "chunk missing frame start", hint);
        } else {
            emitBulkError("ingest", "failed to stage chunk", hint);
        }
        return;
    }

    static StaticJsonDocument<Utility::kMaxBulkConfigSize> doc;
    doc.clear();
    DeserializationError err = deserializeJson(doc, bulkConfigAssembler.payload());
    if (err == DeserializationError::IncompleteInput) {
        return;
    }
    if (err) {
        emitBulkError("parse", err.c_str(), bulkConfigAssembler.sequenceHint());
        bulkConfigAssembler.reset();
        return;
    }

    uint32_t seq = doc["seq"].as<uint32_t>();
    if (seq == 0) {
        seq = bulkConfigAssembler.sequenceHint();
    }
    const char *checksum = doc["checksum"] | nullptr;
    if (!checksum || checksum[0] == '\0') {
        emitBulkError("checksum", "missing checksum", seq);
        bulkConfigAssembler.reset();
        return;
    }

    if (seq == 0) {
        seq = lastAckSequence + 1;
    }

    if (seq == lastAckSequence && lastAckChecksum == checksum) {
        LOG_PRINTLN(Utility::formatAck(checksum, seq));
        bulkConfigAssembler.reset();
        return;
    }

    JsonObject configObj = doc["config"].as<JsonObject>();
    if (!applyConfigObject(configObj, seq)) {
        bulkConfigAssembler.reset();
        return;
    }

    lastAckSequence = seq;
    lastAckChecksum = checksum;
    LOG_PRINTLN(Utility::formatAck(checksum, seq));
    bulkConfigAssembler.reset();
}

void handleSetArgMethodCommand(const ParsedCommand &cmd) {
    const String &command = cmd.fullCommand();
    int method = command.substring(14).toInt();
    if (method >= 0 && method <= static_cast<int>(ARGMethod::XORR)) {
        for (uint8_t slotIndex = 0; slotIndex < NUM_SLOTS; ++slotIndex) {
            MIDISlot &slot = configManager.getSlot(slotIndex);
            slot.arg.method = static_cast<ARGMethod>(method);
            configManager.saveSlot(slotIndex, slot);
        }
        configManager.setARGMethod(static_cast<uint8_t>(method));
        LOG_PRINTLN("OK");
    } else {
        LOG_PRINTLN("ERR");
    }
}

void handleSetEfCommand(const ParsedCommand &cmd) {
    const String &command = cmd.fullCommand();
    int comma = command.indexOf(',');
    if (comma == -1) {
        LOG_PRINTLN("ERR");
        return;
    }
    int potIndex = command.substring(7, comma).toInt();
    int envIndex = command.substring(comma + 1).toInt();
    if (potIndex >= 0 && potIndex < NUM_POTS && envIndex >= 0 &&
        envIndex < static_cast<int>(envelopeFollowers.size())) {
        MIDISlot &slot = configManager.getSlot(static_cast<uint8_t>(potIndex));
        slot.setEnvelopeFollowerIndex(static_cast<int8_t>(envIndex));
        potToEnvelopeMap[potIndex] = slot.efSettings;
        envelopeFollowers[envIndex].toggleActive(true);
        applyEfSettingsToFollower(envelopeFollowers[envIndex], slot.efSettings,
                                  static_cast<uint8_t>(envIndex));
        configManager.saveEnvelopeSettings(potToEnvelopeMap, envelopeFollowers);
        refreshEfVoicesFromConfig();
        LOG_PRINTLN("OK");
    } else {
        LOG_PRINTLN("ERR");
    }
}

void handleSetLedCommand(const ParsedCommand &cmd) {
    const String &command = cmd.fullCommand();
    int first = command.indexOf(',');
    int second = command.indexOf(',', first + 1);
    int third = command.indexOf(',', second + 1);
    if (first == -1 || second == -1 || third == -1) {
        LOG_PRINTLN("ERR");
        return;
    }
    int brightness = command.substring(8, first).toInt();
    int r = command.substring(first + 1, second).toInt();
    int g = command.substring(second + 1, third).toInt();
    int b = command.substring(third + 1).toInt();
    if (brightness >= 0 && brightness <= 255 && r >= 0 && r <= 255 && g >= 0 && g <= 255 &&
        b >= 0 && b <= 255) {
        CRGB color(r, g, b);
        ledManager.setBrightness(brightness);
        ledManager.setColor(color);
        configManager.saveLEDSettings(brightness, color);
        LOG_PRINTLN("OK");
    } else {
        LOG_PRINTLN("ERR");
    }
}

void handleSetPotCommand(const ParsedCommand &cmd) {
    const String &command = cmd.fullCommand();
    int firstComma = command.indexOf(',');
    int lastComma = command.lastIndexOf(',');
    if (firstComma == -1 || lastComma == -1 || firstComma == lastComma) {
        LOG_PRINTLN("Error: Malformed SET_POT command");
        return;
    }
    int potIndex = command.substring(8, firstComma).toInt();
    int channel = command.substring(firstComma + 1, lastComma).toInt();
    int ccNumber = command.substring(lastComma + 1).toInt();
    if (potIndex >= 0 && potIndex < NUM_POTS && channel >= 1 && channel <= 16 && ccNumber >= 0 &&
        ccNumber <= 127) {
        configManager.setPotChannel(potIndex, channel);
        configManager.setPotCCNumber(potIndex, ccNumber);
        potentiometerManager.setChannel(potIndex, channel);
        potentiometerManager.setCCNumber(potIndex, ccNumber);
        if (static_cast<size_t>(potIndex) < potChannels.size()) {
            potChannels[potIndex] = channel;
        }
        configManager.saveConfiguration();
        LOG_PRINTLN("Pot configuration updated!");
    } else {
        LOG_PRINTLN("Error: Invalid values for SET_POT");
    }
}

void handleSetProfileCommand(const ParsedCommand &cmd) {
    const String &command = cmd.fullCommand();
    // Merge incoming JSON onto a captured snapshot so callers may send sparse profile patches
    // instead of a full profile document every time.
    int firstComma = command.indexOf(',');
    int secondComma = command.indexOf(',', firstComma + 1);
    if (firstComma < 0 || secondComma < 0) {
        LOG_PRINTLN("ERR");
        return;
    }
    uint8_t id = static_cast<uint8_t>(command.substring(firstComma + 1, secondComma).toInt());
    if (id >= NUM_PROFILES) {
        LOG_PRINTLN("ERR");
        return;
    }
    String payload = command.substring(secondComma + 1);
    payload.trim();
    if (payload.length() == 0) {
        LOG_PRINTLN("ERR");
        return;
    }
    StaticJsonDocument<12288> doc;
    DeserializationError err = deserializeJson(doc, payload);
    if (err) {
        LOG_PRINTLN("ERR");
        return;
    }
    ProfileData profile = captureProfileSnapshot();
    JsonObject root = doc.as<JsonObject>();

    if (root.containsKey("arp")) {
        JsonObject arp = root["arp"].as<JsonObject>();
        if (arp.containsKey("length_ticks")) {
            profile.arp.lengthTicks = static_cast<uint8_t>(arp["length_ticks"].as<int>());
        }
        if (arp.containsKey("shape")) {
            profile.arp.shape = static_cast<uint8_t>(arp["shape"].as<int>());
        }
        if (arp.containsKey("swing_percent")) {
            profile.arp.swingPercent = static_cast<uint8_t>(arp["swing_percent"].as<int>());
        }
        if (arp.containsKey("gate_percent")) {
            profile.arp.gatePercent = static_cast<uint8_t>(arp["gate_percent"].as<int>());
        }
        if (arp.containsKey("octave_range")) {
            profile.arp.octaveRange = static_cast<uint8_t>(arp["octave_range"].as<int>());
        }
    }

    if (root.containsKey("led")) {
        JsonObject led = root["led"].as<JsonObject>();
        if (led.containsKey("brightness")) {
            profile.led.brightness = static_cast<uint8_t>(led["brightness"].as<int>());
        }
        if (led.containsKey("rgb")) {
            JsonObject rgb = led["rgb"].as<JsonObject>();
            profile.led.r = static_cast<uint8_t>(rgb["r"].as<int>());
            profile.led.g = static_cast<uint8_t>(rgb["g"].as<int>());
            profile.led.b = static_cast<uint8_t>(rgb["b"].as<int>());
        }
    }

    if (root.containsKey("lfos")) {
        JsonArray lfos = root["lfos"].as<JsonArray>();
        for (JsonObject lfo : lfos) {
            uint8_t index = static_cast<uint8_t>(lfo["index"].as<int>());
            if (index >= PROFILE_LFO_COUNT) {
                continue;
            }
            if (lfo.containsKey("shape")) {
                profile.lfos[index].shape = static_cast<uint8_t>(lfo["shape"].as<int>());
            }
            if (lfo.containsKey("frequency_hz")) {
                profile.lfos[index].frequencyHz = lfo["frequency_hz"].as<float>();
            }
            if (lfo.containsKey("depth")) {
                profile.lfos[index].depth = lfo["depth"].as<float>();
            }
            if (lfo.containsKey("bipolar")) {
                profile.lfos[index].bipolar = lfo["bipolar"].as<bool>() ? 1 : 0;
            }
            if (lfo.containsKey("sync")) {
                profile.lfos[index].syncEnabled = lfo["sync"].as<bool>() ? 1 : 0;
            }
            if (lfo.containsKey("sync_ratio")) {
                profile.lfos[index].syncRatio = static_cast<uint8_t>(lfo["sync_ratio"].as<int>());
            }
        }
    }

    profile.routeCount = 0;
    if (root.containsKey("routes")) {
        JsonArray routes = root["routes"].as<JsonArray>();
        for (JsonObject route : routes) {
            if (profile.routeCount >= PROFILE_MAX_ROUTES) {
                break;
            }
            ProfileLfoRoute &out = profile.routes[profile.routeCount];
            out.type = static_cast<uint8_t>(route["type"].as<int>());
            out.lfoIndex = static_cast<uint8_t>(route["lfo"].as<int>());
            out.depth = route["depth"].as<float>();
            out.target = static_cast<uint8_t>(route["target"].as<int>());
            out.channel = static_cast<uint8_t>(route["channel"].as<int>());
            out.ccMsb = static_cast<uint8_t>(route["cc_msb"].as<int>());
            out.ccLsb = static_cast<uint8_t>(route["cc_lsb"].as<int>());
            profile.routeCount++;
        }
    }

    if (root.containsKey("slots")) {
        JsonArray slots = root["slots"].as<JsonArray>();
        for (JsonObject slot : slots) {
            uint8_t index = static_cast<uint8_t>(slot["index"].as<int>());
            if (index >= NUM_SLOTS) {
                continue;
            }
            if (slot.containsKey("channel")) {
                profile.slots[index].midiChannel = static_cast<uint8_t>(slot["channel"].as<int>());
            }
            if (slot.containsKey("ef")) {
                JsonObject ef = slot["ef"].as<JsonObject>();
                parseProfileEf(ef, profile.slots[index].ef);
            }
        }
    }

    if (!configManager.saveProfileSettings(id, profile)) {
        LOG_PRINTLN("ERR");
        return;
    }
    if (id == g_activeProfile) {
        // Keep runtime state in lockstep when the active profile slot is edited remotely.
        ProfileData stored{};
        if (configManager.loadProfileSettings(id, stored)) {
            applyProfileSnapshot(stored, true);
        }
    }
    LOG_PRINTLN("OK");
}

void handleSetSlotValueCommand(const ParsedCommand &cmd) {
    const String &command = cmd.fullCommand();
    int firstComma = command.indexOf(',');
    int lastComma = command.lastIndexOf(',');
    if (firstComma == -1 || lastComma == -1 || firstComma == lastComma) {
        LOG_PRINTLN("ERR");
        return;
    }

    int slotIndex = command.substring(firstComma + 1, lastComma).toInt();
    int midiValue = command.substring(lastComma + 1).toInt();
    if (slotIndex < 0 || slotIndex >= NUM_SLOTS || midiValue < 0 || midiValue > 127) {
        LOG_PRINTLN("ERR");
        return;
    }

    potentiometerManager.injectMidiValue(static_cast<uint8_t>(slotIndex),
                                         static_cast<uint8_t>(midiValue));
    LOG_PRINTLN("OK");
}

} // namespace
#if defined(UNIT_TEST)
bool testOnly_dispatchCommand(const String &command) { return dispatchCommand(command); }
#endif
