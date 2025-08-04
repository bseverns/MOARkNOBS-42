// Handles all persistent configuration stored in EEPROM. This includes MIDI slot
// settings, envelope follower assignments and LED preferences. Backup copies are
// automatically managed to guard against corruption.

#include "ConfigManager.h"
#include "EnvelopeFollower.h"
#include <math.h>

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
    writeEEPROM(false, base);  // Write primary
    writeMagicNumber(false, base);

    // Verify
    std::vector<uint8_t> temp;
    if (!loadConfiguration(temp, base)) {
        Serial.println("Primary EEPROM write failed, saving to backup.");
        writeEEPROM(true, base);
        writeMagicNumber(true, base);
    }
}

// Load configuration (primary)
bool ConfigManager::loadConfiguration(std::vector<uint8_t>& potChannels, uint16_t base) {
    if (checkEEPROMHealth(false, base)) {
        readEEPROM(false, base);
        potChannels.clear();
        for (uint8_t i = 0; i < _numPots; i++) {
            potChannels.push_back(_potChannels[i]);
        }
        return true;
    }
    Serial.println("Primary EEPROM corrupted, trying backup.");
    return loadBackupConfiguration(potChannels, base);
}

// Load configuration (backup)
bool ConfigManager::loadBackupConfiguration(std::vector<uint8_t>& potChannels, uint16_t base) {
    if (checkEEPROMHealth(true, base)) {
        readEEPROM(true, base);
        potChannels.clear();
        for (uint8_t i = 0; i < _numPots; i++) {
            potChannels.push_back(_potChannels[i]);
        }
        return true;
    }
    Serial.println("Backup EEPROM corrupted, resetting to defaults.");
    resetConfiguration(potChannels);
    return false;
}

// Internal read from EEPROM
void ConfigManager::readEEPROM(bool backup, uint16_t base) {
    int offset = base + (backup ? EEPROM_BACKUP_START : EEPROM_START_ADDRESS);
    for (uint8_t i = 0; i < _numPots; i++) {
        _potChannels[i] = EEPROM.read(offset + EEPROM_POT_CHANNELS + i);
        _potCCNumbers[i] = EEPROM.read(offset + EEPROM_POT_CC + i);
    }
}

// Internal write to EEPROM
void ConfigManager::writeEEPROM(bool backup, uint16_t base) {
    int offset = base + (backup ? EEPROM_BACKUP_START : EEPROM_START_ADDRESS);
    for (uint8_t i = 0; i < _numPots; i++) {
        EEPROM.update(offset + EEPROM_POT_CHANNELS + i, _potChannels[i]);
        EEPROM.update(offset + EEPROM_POT_CC + i, _potCCNumbers[i]);
    }
}

// Load a profile block into the current working config
void ConfigManager::loadProfile(uint8_t id) {
    uint16_t base = EEPROM_PROFILE_START(id);
    if (checkEEPROMHealth(false, base)) {
        readEEPROM(false, base);
    } else if (checkEEPROMHealth(true, base)) {
        readEEPROM(true, base);
    } else {
        Serial.println("Profile slot corrupted, using defaults.");
    }
}

// Save the current config into the given profile block
void ConfigManager::saveProfile(uint8_t id) {
    uint16_t base = EEPROM_PROFILE_START(id);
    writeEEPROM(false, base);
    writeMagicNumber(false, base);
    std::vector<uint8_t> temp;
    if (!loadConfiguration(temp, base)) {
        Serial.println("Primary EEPROM write failed, saving to backup.");
        writeEEPROM(true, base);
        writeMagicNumber(true, base);
    }
}

// Initialize configuration
void ConfigManager::begin(std::vector<uint8_t>& potChannels) {
    // 1) Load every MIDISlot from EEPROM into our in-RAM array
    for (uint8_t i = 0; i < NUM_SLOTS; ++i) {
        loadSlot(i, slots[i]);
    }
    // 2) Pull out the existing pot → CC mappings
    //    (assuming _potCCNumbers was filled by readEEPROM)
    potChannels.clear();
    for (uint8_t i = 0; i < _numPots; ++i) {
        potChannels.push_back(_potCCNumbers[i]);
    }
}

void ConfigManager::loadSlot(uint8_t idx, MIDISlot& dest) {
  EEPROM.get(EEPROM_SLOT_BASE + idx * SLOT_EEPROM_SIZE, dest);
  if (dest.arpNote > 127) dest.arpNote = dest.data1;
}

void ConfigManager::saveSlot(uint8_t idx, const MIDISlot& src) {
  EEPROM.put(EEPROM_SLOT_BASE + idx * SLOT_EEPROM_SIZE, src);
}

// Potentiometer accessors
uint8_t ConfigManager::getPotChannel(uint8_t potIndex) const {
    return _potChannels.at(potIndex);
}

uint8_t ConfigManager::getPotCCNumber(uint8_t potIndex) const {
    return _potCCNumbers.at(potIndex);
}

void ConfigManager::setPotChannel(uint8_t potIndex, uint8_t channel) {
    if (potIndex < _numPots) {
        _potChannels[potIndex] = channel;
    }
}

void ConfigManager::setPotCCNumber(uint8_t potIndex, uint8_t ccNumber) {
    if (potIndex < _numPots) {
        _potCCNumbers[potIndex] = ccNumber;
    }
}

// Envelope settings
void ConfigManager::loadEnvelopeSettings(std::map<int, int>& potToEnvelopeMap, std::vector<EnvelopeFollower>& envelopeFollowers) {
    for (size_t i = 0; i < envelopeFollowers.size(); i++) {
        int envelopeIndex = EEPROM.read(EEPROM_ENVELOPE_ASSIGNMENTS + i);
        potToEnvelopeMap[i] = envelopeIndex;
    }
}

void ConfigManager::saveEnvelopeSettings(const std::map<int, int>& potToEnvelopeMap, const std::vector<EnvelopeFollower>& envelopes) {
    for (const auto& [potIndex, envelopeIndex] : potToEnvelopeMap) {
        EEPROM.update(EEPROM_ENVELOPE_ASSIGNMENTS + potIndex, envelopeIndex);
    }
}

void ConfigManager::saveEnvelopeCalibration(uint8_t idx, float baseline) {
    EEPROM.put(EEPROM_EF_BASELINES + idx * sizeof(float), baseline);
}

bool ConfigManager::loadEnvelopeCalibrations(std::vector<EnvelopeFollower>& envelopes) {
    bool found = false;
    for (size_t i = 0; i < envelopes.size(); ++i) {
        float b;
        EEPROM.get(EEPROM_EF_BASELINES + i * sizeof(float), b);
        if (!isnan(b)) {
            envelopes[i].setBaseline(b);
            envelopes[i].setVref(g_vref);
            found = true;
        }
    }
    return found;
}

// LED settings
void ConfigManager::loadLEDSettings(uint8_t& brightness, CRGB& color) {
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
void ConfigManager::resetConfiguration(std::vector<uint8_t>& potChannels) {
    potChannels.clear();
    for (uint8_t i = 0; i < _numPots; i++) {
        setPotChannel(i, 1); // Default to channel 1
        setPotCCNumber(i, 0); // Default to CC 0
    }
    saveConfiguration();
}

// Mode and ARG methods
void ConfigManager::setMode(uint8_t mode) {
    EEPROM.update(EEPROM_ARG_MODE, mode);
}

uint8_t ConfigManager::getMode() const {
    return EEPROM.read(EEPROM_ARG_MODE);
}

void ConfigManager::setARGMethod(uint8_t method) {
    EEPROM.update(EEPROM_ARG_METHOD, method);
}

uint8_t ConfigManager::getARGMethod() const {
    return EEPROM.read(EEPROM_ARG_METHOD);
}

void ConfigManager::setEnvelopePair(uint8_t envA, uint8_t envB) {
    EEPROM.update(EEPROM_ARG_ENV_A, envA);
    EEPROM.update(EEPROM_ARG_ENV_B, envB);
}

uint8_t ConfigManager::getEnvelopeA() const {
    return EEPROM.read(EEPROM_ARG_ENV_A);
}

uint8_t ConfigManager::getEnvelopeB() const {
    return EEPROM.read(EEPROM_ARG_ENV_B);
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
    s += "}";  // properties
    s += "}";  // root object

    return s;
}

String ConfigManager::serializeAll() const {
    String output = "{ \"pots\": [";

    for (uint8_t i = 0; i < _numPots; ++i) {
        output += "{";
        output += "\"channel\": ";
        output += _potChannels.at(i);
        output += ", \"cc\": ";
        output += _potCCNumbers.at(i);
        output += "}";

        if (i < _numPots - 1) {
            output += ",";
        }
    }

    output += "] }";
    return output;
}

void ConfigManager::saveMIDISlots(const MIDISlot* slots, size_t count) {
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

void ConfigManager::loadMIDISlots(MIDISlot* slots, size_t count) {
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

bool ConfigManager::handleCommand(const String& command) {
    if (command.startsWith("GET_FILTER")) {
        uint8_t type = EEPROM.read(EEPROM_ENVELOPE_TYPES);
        float freq, q;
        EEPROM.get(EEPROM_FILTER_FREQ, freq);
        EEPROM.get(EEPROM_FILTER_Q, q);
        Serial.print(type);
        Serial.print(",");
        Serial.print(freq, 2);
        Serial.print(",");
        Serial.println(q, 2);
        return true;
    } else if (command.startsWith("SET_FILTER")) {
        int firstComma = command.indexOf(',');
        int secondComma = command.indexOf(',', firstComma + 1);
        if (firstComma == -1 || secondComma == -1) {
            Serial.println("ERR");
            return true;
        }
        uint8_t type = command.substring(10, firstComma).toInt();
        float freq = command.substring(firstComma + 1, secondComma).toFloat();
        float q = command.substring(secondComma + 1).toFloat();
        EEPROM.update(EEPROM_ENVELOPE_TYPES, type);
        EEPROM.put(EEPROM_FILTER_FREQ, freq);
        EEPROM.put(EEPROM_FILTER_Q, q);
        Serial.println("OK");
        return true;
    } else if (command.startsWith("GET_ARGPAIR")) {
        Serial.print(getEnvelopeA());
        Serial.print(",");
        Serial.println(getEnvelopeB());
        return true;
    } else if (command.startsWith("SET_ARGPAIR")) {
        int comma = command.indexOf(',');
        if (comma == -1) {
            Serial.println("ERR");
            return true;
        }
        uint8_t envA = command.substring(11, comma).toInt();
        uint8_t envB = command.substring(comma + 1).toInt();
        setEnvelopePair(envA, envB);
        Serial.println("OK");
        return true;
    }
    return false;
}
