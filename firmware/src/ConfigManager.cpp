// Handles all persistent configuration stored in EEPROM. This includes MIDI slot
// settings, envelope follower assignments and LED preferences. Backup copies are
// automatically managed to guard against corruption.

#include "ConfigManager.h"
#include "EnvelopeFollower.h"
#include "ARGMixer.h"
#include <cmath>
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
EnvelopeFollower::FilterType toEnvelopeFilter(MIDISlot::EfSettings::FilterType type) {
    using Filter = MIDISlot::EfSettings::FilterType;
    switch (type) {
    case Filter::Linear:
        return EnvelopeFollower::LINEAR;
    case Filter::OppositeLinear:
        return EnvelopeFollower::OPPOSITE_LINEAR;
    case Filter::Exponential:
        return EnvelopeFollower::EXPONENTIAL;
    case Filter::Random:
        return EnvelopeFollower::RANDOM;
    case Filter::Lowpass:
        return EnvelopeFollower::LOWPASS;
    case Filter::Highpass:
        return EnvelopeFollower::HIGHPASS;
    case Filter::Bandpass:
        return EnvelopeFollower::BANDPASS;
    }
    return EnvelopeFollower::LINEAR;
}

void applyEfSettingsToFollower(EnvelopeFollower &ef, const MIDISlot::EfSettings &settings) {
    ef.setFilterType(toEnvelopeFilter(settings.filterType));
    ef.configureFilter(settings.frequency, settings.q);
    ef.setOversampleCount(settings.oversample);
    ef.setSmoothingAlpha(settings.smoothing);
    ef.setBaseline(settings.baseline);
    ef.setGain(settings.gain);
}

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
} // namespace

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

// Constructor
ConfigManager::ConfigManager(uint8_t numPots, uint8_t numButtons)
    : _numPots(numPots), _numButtons(numButtons) {}

// Centralized EEPROM health check
bool ConfigManager::checkEEPROMHealth(bool backup, uint16_t base) {
    int address = base + (backup ? EEPROM_MAGIC_ADDRESS + 2 : EEPROM_MAGIC_ADDRESS);
    uint16_t magic = EEPROM.read(address) << 8 | EEPROM.read(address + 1);
    return (magic == (backup ? EEPROM_MAGIC_BACKUP : EEPROM_MAGIC_PRIMARY));
}

// Write magic number to EEPROM
void ConfigManager::writeMagicNumber(bool backup, uint16_t base) {
    int address = base + (backup ? EEPROM_MAGIC_ADDRESS + 2 : EEPROM_MAGIC_ADDRESS);
    uint16_t magic = backup ? EEPROM_MAGIC_BACKUP : EEPROM_MAGIC_PRIMARY;
    EEPROM.update(address, (magic >> 8) & 0xFF);
    EEPROM.update(address + 1, magic & 0xFF);
}

// Save configuration with verification and backup
void ConfigManager::saveConfiguration() {
    uint16_t base = EEPROM_PROFILE_START(0);
    writeEEPROM(false, base); // Write primary
    writeMagicNumber(false, base);

    // Verify
    std::vector<uint8_t> temp;
    if (!loadConfiguration(temp, base)) {
        LOG_PRINTLN("Primary EEPROM write failed, saving to backup.");
        writeEEPROM(true, base);
        writeMagicNumber(true, base);
    }
}

// Load configuration (primary)
bool ConfigManager::loadConfiguration(std::vector<uint8_t> &potChannels, uint16_t base) {
    if (checkEEPROMHealth(false, base)) {
        readEEPROM(false, base);
        if (_stored.version != CONFIG_VERSION || _stored.crc != calculateCRC()) {
            LOG_PRINTLN("Config CRC/version mismatch.");
            resetConfiguration(potChannels);
            return false;
        }
        potChannels.clear();
        for (uint8_t i = 0; i < _numPots; i++) {
            potChannels.push_back(_stored.potChannels[i]);
        }
        return true;
    }
    LOG_PRINTLN("Primary EEPROM corrupted, trying backup.");
    return loadBackupConfiguration(potChannels, base);
}

// Load configuration (backup)
bool ConfigManager::loadBackupConfiguration(std::vector<uint8_t> &potChannels, uint16_t base) {
    if (checkEEPROMHealth(true, base)) {
        readEEPROM(true, base);
        if (_stored.version != CONFIG_VERSION || _stored.crc != calculateCRC()) {
            LOG_PRINTLN("Backup CRC/version mismatch.");
            resetConfiguration(potChannels);
            return false;
        }
        potChannels.clear();
        for (uint8_t i = 0; i < _numPots; i++) {
            potChannels.push_back(_stored.potChannels[i]);
        }
        return true;
    }
    LOG_PRINTLN("Backup EEPROM corrupted, resetting to defaults.");
    resetConfiguration(potChannels);
    return false;
}

// Internal read from EEPROM
void ConfigManager::readEEPROM(bool backup, uint16_t base) {
    int offset = base + (backup ? EEPROM_BACKUP_START : EEPROM_START_ADDRESS);
    for (uint8_t i = 0; i < _numPots; i++) {
        _stored.potChannels[i] = EEPROM.read(offset + EEPROM_POT_CHANNELS + i);
        _stored.potCCNumbers[i] = EEPROM.read(offset + EEPROM_POT_CC + i);
    }
    EEPROM.get(offset + EEPROM_CONFIG_VERSION, _stored.version);
    EEPROM.get(offset + EEPROM_CONFIG_CRC, _stored.crc);
}

// Internal write to EEPROM
void ConfigManager::writeEEPROM(bool backup, uint16_t base) {
    int offset = base + (backup ? EEPROM_BACKUP_START : EEPROM_START_ADDRESS);
    uint16_t crc = calculateCRC();
    for (uint8_t i = 0; i < _numPots; i++) {
        EEPROM.update(offset + EEPROM_POT_CHANNELS + i, _stored.potChannels[i]);
        EEPROM.update(offset + EEPROM_POT_CC + i, _stored.potCCNumbers[i]);
    }
    EEPROM.put(offset + EEPROM_CONFIG_VERSION, (uint16_t)CONFIG_VERSION);
    EEPROM.put(offset + EEPROM_CONFIG_CRC, crc);
    _stored.version = CONFIG_VERSION;
    _stored.crc = crc;
}

uint16_t ConfigManager::calculateCRC() const {
    uint16_t crc = 0xFFFF;
    for (uint8_t i = 0; i < _numPots; ++i) {
        crc = crc16_update(crc, _stored.potChannels[i]);
    }
    for (uint8_t i = 0; i < _numPots; ++i) {
        crc = crc16_update(crc, _stored.potCCNumbers[i]);
    }
    return crc;
}

// Load a profile block into the current working config
void ConfigManager::loadProfile(uint8_t id) {
    uint16_t base = EEPROM_PROFILE_START(id);
    if (checkEEPROMHealth(false, base)) {
        readEEPROM(false, base);
        if (_stored.version != CONFIG_VERSION || _stored.crc != calculateCRC()) {
            LOG_PRINTLN("Profile slot corrupted, using defaults.");
        }
    } else if (checkEEPROMHealth(true, base)) {
        readEEPROM(true, base);
        if (_stored.version != CONFIG_VERSION || _stored.crc != calculateCRC()) {
            LOG_PRINTLN("Profile slot corrupted, using defaults.");
        }
    } else {
        LOG_PRINTLN("Profile slot corrupted, using defaults.");
    }
}

// Save the current config into the given profile block
void ConfigManager::saveProfile(uint8_t id) {
    uint16_t base = EEPROM_PROFILE_START(id);
    writeEEPROM(false, base);
    writeMagicNumber(false, base);
    std::vector<uint8_t> temp;
    if (!loadConfiguration(temp, base)) {
        LOG_PRINTLN("Primary EEPROM write failed, saving to backup.");
        writeEEPROM(true, base);
        writeMagicNumber(true, base);
    }
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
    EEPROM.get(address, temp);
    if (temp.sysexLength > SysExTemplate::kMaxLength) {
        temp.sysexLength = 0;
        temp.sysexTemplate.fill(0);
    }
    temp.arg = sanitizeArgConfig(temp.arg);
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
    sanitized.arg = sanitizeArgConfig(sanitized.arg);
    const int address = static_cast<int>(EEPROM_SLOT_BASE + idx * SLOT_EEPROM_SIZE);
    EEPROM.put(address, sanitized);
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
    }
}

// Envelope settings
bool ConfigManager::loadEnvelopeSettings(std::map<int, MIDISlot::EfSettings> &potToEnvelopeMap,
                                         std::vector<EnvelopeFollower> &envelopes) {
    bool allFound = true;

    potToEnvelopeMap.clear();
    for (uint8_t potIndex = 0; potIndex < NUM_POTS; ++potIndex) {
        int storedValue = EEPROM.read(EEPROM_ENVELOPE_ASSIGNMENTS + potIndex);
        int envelopeIndex = (storedValue == 0xFF) ? kUnassignedEnvelope : storedValue;
        MIDISlot::EfSettings settings = {};
        if (potIndex < slots.size()) {
            settings = slots[potIndex].ef;
        }
        if (envelopeIndex >= 0 && envelopeIndex < static_cast<int>(envelopes.size())) {
            settings.followerIndex = static_cast<int8_t>(envelopeIndex);
            potToEnvelopeMap.emplace(potIndex, settings);
            if (potIndex < slots.size()) {
                slots[potIndex].ef = settings;
            }
        } else if (potIndex < slots.size()) {
            slots[potIndex].ef.followerIndex = -1;
        }
    }

    for (size_t envIndex = 0; envIndex < envelopes.size(); ++envIndex) {
        float baseline;
        EEPROM.get(EEPROM_EF_BASELINES + envIndex * sizeof(float), baseline);

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
        applyEfSettingsToFollower(envelopes[follower], settings);
        if (static_cast<size_t>(entry.first) < slots.size()) {
            slots[entry.first].ef = settings;
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
        EEPROM.update(EEPROM_ENVELOPE_ASSIGNMENTS + potIndex, storedValue);

        if (potIndex < slots.size()) {
            MIDISlot &slot = slots[potIndex];
            if (it != potToEnvelopeMap.end()) {
                slot.ef = it->second;
                if (envelopeIndex >= 0 && envelopeIndex < static_cast<int>(envelopes.size())) {
                    slot.ef.followerIndex = static_cast<int8_t>(envelopeIndex);
                    slot.ef.baseline = envelopes[envelopeIndex].getBaseline();
                } else {
                    slot.ef.followerIndex = -1;
                }
            } else {
                slot.ef.followerIndex = -1;
            }
        }
    }
    for (size_t i = 0; i < envelopes.size(); ++i) {
        envelopeConfig.baselines[i] = envelopes[i].getBaseline();
        EEPROM.put(EEPROM_EF_BASELINES + i * sizeof(float), envelopeConfig.baselines[i]);
    }
}

void ConfigManager::saveEnvelopeBaseline(uint8_t envIndex, float baseline) {
    if (envIndex < NUM_ENVELOPES) {
        envelopeConfig.baselines[envIndex] = baseline;
        EEPROM.put(EEPROM_EF_BASELINES + envIndex * sizeof(float), baseline);
    }
}

// LED settings
void ConfigManager::loadLEDSettings(uint8_t &brightness, CRGB &color) {
    brightness = EEPROM.read(EEPROM_LED_BRIGHTNESS);
    color.r = EEPROM.read(EEPROM_LED_COLOR);
    color.g = EEPROM.read(EEPROM_LED_COLOR + 1);
    color.b = EEPROM.read(EEPROM_LED_COLOR + 2);
}

void ConfigManager::saveLEDSettings(uint8_t brightness, CRGB color) {
    EEPROM.update(EEPROM_LED_BRIGHTNESS, brightness);
    EEPROM.update(EEPROM_LED_COLOR, color.r);
    EEPROM.update(EEPROM_LED_COLOR + 1, color.g);
    EEPROM.update(EEPROM_LED_COLOR + 2, color.b);
}

// Reset configuration to defaults
void ConfigManager::resetConfiguration(std::vector<uint8_t> &potChannels) {
    potChannels.clear();
    for (uint8_t i = 0; i < _numPots; i++) {
        setPotChannel(i, 1);  // Default to channel 1
        setPotCCNumber(i, 0); // Default to CC 0
    }
    saveConfiguration();
}

// Mode and ARG methods
void ConfigManager::setMode(uint8_t mode) {
    legacyArg.mode = mode;
    EEPROM.update(EEPROM_ARG_MODE, legacyArg.mode);
}

uint8_t ConfigManager::getMode() const { return legacyArg.mode; }

void ConfigManager::setARGMethod(uint8_t method) {
    legacyArg.method = method;
    SlotARGConfig defaults{};
    defaults.enabled = legacyArg.enable;
    defaults.method = static_cast<ARGMethod>(legacyArg.method);
    defaults.sourceA = legacyArg.sourceA;
    defaults.sourceB = legacyArg.sourceB;
    defaults = sanitizeArgConfig(defaults);
    legacyArg.method = static_cast<uint8_t>(defaults.method);
    EEPROM.update(EEPROM_ARG_METHOD, legacyArg.method);
}

uint8_t ConfigManager::getARGMethod() const { return legacyArg.method; }

void ConfigManager::setARGEnable(uint8_t enable) {
    legacyArg.enable = enable ? 1 : 0;
    EEPROM.update(EEPROM_ARG_ENABLE, legacyArg.enable);
}

uint8_t ConfigManager::getARGEnable() const { return legacyArg.enable; }

void ConfigManager::setEnvelopePair(uint8_t envA, uint8_t envB) {
    int idxA = envelopeIndexFromAnalogPin(envA);
    if (idxA < 0 && envA < NUM_ENVELOPES) {
        idxA = envA;
    }
    int idxB = envelopeIndexFromAnalogPin(envB);
    if (idxB < 0 && envB < NUM_ENVELOPES) {
        idxB = envB;
    }
    if (idxA < 0)
        idxA = legacyArg.sourceA;
    if (idxB < 0)
        idxB = legacyArg.sourceB;

    legacyArg.sourceA = static_cast<uint8_t>(idxA % NUM_ENVELOPES);
    legacyArg.sourceB = static_cast<uint8_t>(idxB % NUM_ENVELOPES);
    if (legacyArg.sourceA == legacyArg.sourceB) {
        legacyArg.sourceB = (legacyArg.sourceA + 1) % NUM_ENVELOPES;
    }

    EEPROM.update(EEPROM_ARG_ENV_A, legacyArg.sourceA);
    EEPROM.update(EEPROM_ARG_ENV_B, legacyArg.sourceB);
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
    const uint8_t count = NUM_POTS;

    String s = "{";
    s += "\"type\": \"object\",";
    s += "\"properties\": {";
    s += "\"pots\": {";
    s += "\"type\": \"array\",";
    s += "\"items\": {\"type\": \"number\"},";
    s += "\"count\": ";
    s += String(count);
    s += ",";
    s += "\"minItems\": ";
    s += String(count);
    s += ",";
    s += "\"maxItems\": ";
    s += String(count);
    s += "}";
    s += "}"; // properties
    s += "}"; // root object

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

SlotARGConfig ConfigManager::sanitizeArgConfig(const SlotARGConfig &candidate) {
    return sanitizeSlotArg(candidate);
}

void ConfigManager::loadLegacyARGSettings() {
    legacyArg.mode = EEPROM.read(EEPROM_ARG_MODE);
    legacyArg.method = EEPROM.read(EEPROM_ARG_METHOD);
    legacyArg.enable = EEPROM.read(EEPROM_ARG_ENABLE);

    const uint8_t rawA = EEPROM.read(EEPROM_ARG_ENV_A);
    const uint8_t rawB = EEPROM.read(EEPROM_ARG_ENV_B);

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
    defaults = sanitizeArgConfig(defaults);

    legacyArg.enable = defaults.enabled;
    legacyArg.method = static_cast<uint8_t>(defaults.method);
    legacyArg.sourceA = defaults.sourceA;
    legacyArg.sourceB = defaults.sourceB;
}

void ConfigManager::migrateLegacyARGSettings() {
    loadLegacyARGSettings();

    uint16_t storedVersion = 0;
    EEPROM.get(EEPROM_CONFIG_VERSION, storedVersion);

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
            EEPROM.get(legacyAddress, legacySlots[i]);
        }

        SlotARGConfig defaults{};
        defaults.enabled = legacyArg.enable;
        defaults.method = static_cast<ARGMethod>(legacyArg.method);
        defaults.sourceA = legacyArg.sourceA;
        defaults.sourceB = legacyArg.sourceB;
        defaults = sanitizeArgConfig(defaults);

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

        EEPROM.update(EEPROM_ARG_MODE, legacyArg.mode);
        EEPROM.update(EEPROM_ARG_METHOD, legacyArg.method);
        EEPROM.update(EEPROM_ARG_ENABLE, legacyArg.enable);
        EEPROM.update(EEPROM_ARG_ENV_A, legacyArg.sourceA);
        EEPROM.update(EEPROM_ARG_ENV_B, legacyArg.sourceB);
        EEPROM.put(EEPROM_CONFIG_VERSION, static_cast<uint16_t>(CONFIG_VERSION));
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
    if (candidate.efSettings.followerIndex < -1 ||
        candidate.efSettings.followerIndex >= static_cast<int>(NUM_ENVELOPES)) {
        return false;
    }
    SlotARGConfig sanitized = sanitizeArgConfig(candidate.arg);
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
    EEPROM.get(EEPROM_CONFIG_VERSION, storedVersion);

    MIDISlot candidate{};
    EEPROM.get(static_cast<int>(EEPROM_SLOT_BASE), candidate);

    if (!slotLooksSane(candidate)) {
        wipeSlotRegion();
        wipeProfileBlocks();
        storedVersion = CONFIG_VERSION;
    }

    if (storedVersion != CONFIG_VERSION) {
        wipeSlotRegion();
        wipeProfileBlocks();
        EEPROM.put(EEPROM_CONFIG_VERSION, static_cast<uint16_t>(CONFIG_VERSION));
    }
}

void ConfigManager::wipeSlotRegion() {
    MIDISlot blank{};
    blank.midiChannel = 1;
    blank.arg = sanitizeArgConfig(blank.arg);
    slots.fill(blank);

    for (uint8_t i = 0; i < NUM_SLOTS; ++i) {
        const int address = static_cast<int>(EEPROM_SLOT_BASE + i * SLOT_EEPROM_SIZE);
        EEPROM.put(address, blank);
    }

    legacyArg.enable = 0;
    legacyArg.method = static_cast<uint8_t>(ARGMethod::PLUS);
    legacyArg.sourceA = 0;
    legacyArg.sourceB = 1;
    EEPROM.update(EEPROM_ARG_ENABLE, legacyArg.enable);
    EEPROM.update(EEPROM_ARG_METHOD, legacyArg.method);
    EEPROM.update(EEPROM_ARG_ENV_A, legacyArg.sourceA);
    EEPROM.update(EEPROM_ARG_ENV_B, legacyArg.sourceB);
}

void ConfigManager::wipeProfileBlocks() {
    constexpr uint8_t kProfileCount = 3; // primary + two alternates in the UI cycle
    for (uint8_t id = 1; id < kProfileCount; ++id) {
        const uint16_t base = EEPROM_PROFILE_START(id);
        for (uint16_t offset = 0; offset < EEPROM_PROFILE_BLOCK_SIZE; ++offset) {
            EEPROM.update(static_cast<int>(base + offset), 0x00);
        }
    }
}

bool ConfigManager::handleCommand(const String &command) {
    if (command.startsWith("CAL_ENVS")) {
        for (auto &ef : envelopeFollowers) {
            ef.calibrate();
        }
        LOG_PRINTLN("OK");
        return true;
    } else if (command.startsWith("GET_FILTER")) {
        const uint8_t efType = EEPROM.read(EEPROM_ENVELOPE_TYPES);
        (void)efType; // keep the compiler chill
        float freq, q;
        EEPROM.get(EEPROM_FILTER_FREQ, freq);
        EEPROM.get(EEPROM_FILTER_Q, q);
        LOG_PRINT(efType);
        LOG_PRINT(",");
        LOG_PRINT(freq, 2);
        LOG_PRINT(",");
        LOG_PRINTLN(q, 2);
        return true;
    } else if (command.startsWith("SET_FILTER")) {
        int firstComma = command.indexOf(',');
        int secondComma = command.indexOf(',', firstComma + 1);
        if (firstComma == -1 || secondComma == -1) {
            LOG_PRINTLN("ERR");
            return true;
        }
        uint8_t efType = command.substring(10, firstComma).toInt();
        float freq = command.substring(firstComma + 1, secondComma).toFloat();
        float q = command.substring(secondComma + 1).toFloat();
        EEPROM.update(EEPROM_ENVELOPE_TYPES, efType);
        EEPROM.put(EEPROM_FILTER_FREQ, freq);
        EEPROM.put(EEPROM_FILTER_Q, q);
        LOG_PRINTLN("OK");
        return true;
    } else if (command.startsWith("GET_ARGPAIR")) {
        LOG_PRINT(getARGEnable());
        LOG_PRINT(",");
        LOG_PRINT(getEnvelopeA());
        LOG_PRINT(",");
        LOG_PRINTLN(getEnvelopeB());
        return true;
    } else if (command.startsWith("SET_ARGPAIR")) {
        int first = command.indexOf(',');
        int second = command.indexOf(',', first + 1);
        if (first == -1 || second == -1) {
            LOG_PRINTLN("ERR");
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
        LOG_PRINTLN("OK");
        return true;
    }
    return false;
}

#ifdef UNIT_TEST
#include <unity.h>
#include "TestHelpers.h"

// Ensure the manager can resurrect configuration from the backup copy
// after the primary EEPROM header gets nuked.
void test_eeprom_recovery_after_power_cycle() {
    ConfigManager cfg(NUM_POTS, NUM_BUTTONS);
    cfg.setPotChannel(0, 9);
    cfg.setPotCCNumber(0, 77);
    cfg.saveConfiguration();

    // Mirror primary data into the backup region and wreck the primary header
    EEPROM.write(EEPROM_MAGIC_ADDRESS + 2, (EEPROM_MAGIC_BACKUP >> 8) & 0xFF);
    EEPROM.write(EEPROM_MAGIC_ADDRESS + 3, EEPROM_MAGIC_BACKUP & 0xFF);
    EEPROM.write(EEPROM_BACKUP_START + EEPROM_POT_CHANNELS, 9);
    EEPROM.write(EEPROM_BACKUP_START + EEPROM_POT_CC, 77);
    EEPROM.write(EEPROM_MAGIC_ADDRESS, 0x00);
    EEPROM.write(EEPROM_MAGIC_ADDRESS + 1, 0x00);

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

    std::map<int, int> mapping;
    const int highPot = NUM_POTS - 1;
    const int assignedEnv = static_cast<int>(envs.size()) - 1;
    mapping[highPot] = assignedEnv;

    ConfigManager cfg(NUM_POTS, NUM_BUTTONS);
    cfg.saveEnvelopeSettings(mapping, envs);

    mapping.clear();
    auto reloaded = createEnvelopeFollowers(&pm);
    bool ok = cfg.loadEnvelopeSettings(mapping, reloaded);

    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_UINT(1u, mapping.size());

    auto highPotIt = mapping.find(highPot);
    TEST_ASSERT_TRUE(highPotIt != mapping.end());
    TEST_ASSERT_EQUAL_INT(assignedEnv, highPotIt->second);

    if (NUM_POTS > 1) {
        int unassignedPot = (highPot == 0) ? 1 : 0;
        auto unassignedIt = mapping.find(unassignedPot);
        TEST_ASSERT_TRUE(unassignedIt == mapping.end());
    }
}
#endif
