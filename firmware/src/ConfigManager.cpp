// Handles all persistent configuration stored in EEPROM. This includes MIDI slot
// settings, envelope follower assignments and LED preferences. Backup copies are
// automatically managed to guard against corruption.

#include "ConfigManager.h"
#include "EnvelopeFollower.h"
#include <cmath>
#include <vector>
#include "Log.h"

extern std::vector<EnvelopeFollower> envelopeFollowers;

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
    // 1) Load every MIDISlot from EEPROM into our in-RAM array
    for (uint8_t i = 0; i < NUM_SLOTS; ++i) {
        loadSlot(i, slots[i]);
    }
    // 2) Pull out the existing pot → CC mappings
    //    (assuming _stored.potCCNumbers was filled by readEEPROM)
    potChannels.clear();
    for (uint8_t i = 0; i < _numPots; ++i) {
        potChannels.push_back(_stored.potCCNumbers[i]);
    }
}

void ConfigManager::loadSlot(uint8_t idx, MIDISlot &dest) {
    EEPROM.get(EEPROM_SLOT_BASE + idx * SLOT_EEPROM_SIZE, dest);
    if (dest.arpNote > 127)
        dest.arpNote = dest.data1;
}

void ConfigManager::saveSlot(uint8_t idx, const MIDISlot &src) {
    EEPROM.put(EEPROM_SLOT_BASE + idx * SLOT_EEPROM_SIZE, src);
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
    }
}

void ConfigManager::setPotCCNumber(uint8_t potIndex, uint8_t ccNumber) {
    if (potIndex < _numPots) {
        _stored.potCCNumbers[potIndex] = ccNumber;
    }
}

// Envelope settings
bool ConfigManager::loadEnvelopeSettings(std::map<int, int> &potToEnvelopeMap,
                                         std::vector<EnvelopeFollower> &envelopes) {
    bool allFound = true;
    for (size_t i = 0; i < envelopes.size(); i++) {
        int envelopeIndex = EEPROM.read(EEPROM_ENVELOPE_ASSIGNMENTS + i);
        potToEnvelopeMap[i] = envelopeIndex;

        float b;
        EEPROM.get(EEPROM_EF_BASELINES + i * sizeof(float), b);

        envelopes[i].setVref(g_vref); // always refresh Vref
        if (!std::isnan(b)) {
            envelopes[i].setBaseline(b);
            envelopeConfig.baselines[i] = b;
        } else {
            envelopeConfig.baselines[i] = 0.0f;
            allFound = false;
        }
    }
    return allFound;
}

void ConfigManager::saveEnvelopeSettings(const std::map<int, int> &potToEnvelopeMap,
                                         const std::vector<EnvelopeFollower> &envelopes) {
    for (const auto &[potIndex, envelopeIndex] : potToEnvelopeMap) {
        EEPROM.update(EEPROM_ENVELOPE_ASSIGNMENTS + potIndex, envelopeIndex);
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
void ConfigManager::setMode(uint8_t mode) { EEPROM.update(EEPROM_ARG_MODE, mode); }

uint8_t ConfigManager::getMode() const { return EEPROM.read(EEPROM_ARG_MODE); }

void ConfigManager::setARGMethod(uint8_t method) { EEPROM.update(EEPROM_ARG_METHOD, method); }

uint8_t ConfigManager::getARGMethod() const { return EEPROM.read(EEPROM_ARG_METHOD); }

void ConfigManager::setARGEnable(uint8_t enable) { EEPROM.update(EEPROM_ARG_ENABLE, enable); }

uint8_t ConfigManager::getARGEnable() const { return EEPROM.read(EEPROM_ARG_ENABLE); }

void ConfigManager::setEnvelopePair(uint8_t envA, uint8_t envB) {
    EEPROM.update(EEPROM_ARG_ENV_A, envA);
    EEPROM.update(EEPROM_ARG_ENV_B, envB);
}

uint8_t ConfigManager::getEnvelopeA() const { return EEPROM.read(EEPROM_ARG_ENV_A); }

uint8_t ConfigManager::getEnvelopeB() const { return EEPROM.read(EEPROM_ARG_ENV_B); }

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
        int address = EEPROM_SLOT_BASE + i * SLOT_EEPROM_SIZE;
        EEPROM.put(address, slots[i]);
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
        int address = EEPROM_SLOT_BASE + i * SLOT_EEPROM_SIZE;
        EEPROM.get(address, slots[i]);
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
    std::map<int, int> mapping = {{0, 0}};

    envs[0].setBaseline(0.42f);
    ConfigManager cfg(NUM_POTS, NUM_BUTTONS);
    cfg.saveEnvelopeSettings(mapping, envs);

    // Pretend the board restarted – wipe RAM and reload from EEPROM
    for (int i = 0; i < NUM_ENVELOPES; ++i) {
        envelopeConfig.baselines[i] = 0.0f;
    }
    std::vector<EnvelopeFollower> fresh = {EnvelopeFollower(A0, &pm, 0)};
    std::map<int, int> mapping2;
    bool ok = cfg.loadEnvelopeSettings(mapping2, fresh);

    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.42f, fresh[0].getBaseline());
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.42f, envelopeConfig.baselines[0]);
}
#endif
