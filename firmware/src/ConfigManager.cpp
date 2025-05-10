// ConfigManager.cpp — Updated with EEPROM robustness and backup handling, preserving development comments

#include "ConfigManager.h"

// Constructor
ConfigManager::ConfigManager(uint8_t numPots, uint8_t numButtons)
    : _numPots(numPots), _numButtons(numButtons) {}

// Centralized EEPROM health check
bool ConfigManager::checkEEPROMHealth(bool backup) {
    int address = backup ? EEPROM_MAGIC_ADDRESS + 2 : EEPROM_MAGIC_ADDRESS;
    uint16_t magic = EEPROM.read(address) << 8 | EEPROM.read(address + 1);
    return (magic == (backup ? EEPROM_MAGIC_BACKUP : EEPROM_MAGIC_PRIMARY));
}

// Write magic number to EEPROM
void ConfigManager::writeMagicNumber(bool backup) {
    int address = backup ? EEPROM_MAGIC_ADDRESS + 2 : EEPROM_MAGIC_ADDRESS;
    uint16_t magic = backup ? EEPROM_MAGIC_BACKUP : EEPROM_MAGIC_PRIMARY;
    EEPROM.update(address, (magic >> 8) & 0xFF);
    EEPROM.update(address + 1, magic & 0xFF);
}

// Save configuration with verification and backup
void ConfigManager::saveConfiguration() {
    writeEEPROM(false);  // Write primary
    writeMagicNumber(false);

    // Verify
    std::vector<uint8_t> temp;
    if (!loadConfiguration(temp)) {
        Serial.println("Primary EEPROM write failed, saving to backup.");
        writeEEPROM(true);
        writeMagicNumber(true);
    }
}

// Load configuration (primary)
bool ConfigManager::loadConfiguration(std::vector<uint8_t>& potChannels) {
    if (checkEEPROMHealth(false)) {
        readEEPROM(false);
        potChannels.clear();
        for (uint8_t i = 0; i < _numPots; i++) {
            potChannels.push_back(_potChannels[i]);
        }
        return true;
    }
    Serial.println("Primary EEPROM corrupted, trying backup.");
    return loadBackupConfiguration(potChannels);
}

// Load configuration (backup)
bool ConfigManager::loadBackupConfiguration(std::vector<uint8_t>& potChannels) {
    if (checkEEPROMHealth(true)) {
        readEEPROM(true);
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
void ConfigManager::readEEPROM(bool backup) {
    int offset = backup ? EEPROM_BACKUP_START : EEPROM_START_ADDRESS;
    for (uint8_t i = 0; i < _numPots; i++) {
        _potChannels[i] = EEPROM.read(offset + EEPROM_POT_CHANNELS + i);
        _potCCNumbers[i] = EEPROM.read(offset + EEPROM_POT_CC + i);
    }
}

// Internal write to EEPROM
void ConfigManager::writeEEPROM(bool backup) {
    int offset = backup ? EEPROM_BACKUP_START : EEPROM_START_ADDRESS;
    for (uint8_t i = 0; i < _numPots; i++) {
        EEPROM.update(offset + EEPROM_POT_CHANNELS + i, _potChannels[i]);
        EEPROM.update(offset + EEPROM_POT_CC + i, _potCCNumbers[i]);
    }
}

// Initialize configuration
void ConfigManager::begin(std::vector<uint8_t>& potChannels) {
    loadConfiguration(potChannels);
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
