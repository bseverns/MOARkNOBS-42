#include "ConfigManager.h"

ConfigManager::ConfigManager(uint8_t numPots, uint8_t numButtons)
    : _numPots(numPots), _numButtons(numButtons) {}

void ConfigManager::begin(std::vector<uint8_t>& potChannels) {
    if (!loadConfiguration(potChannels)) {
        Serial.println("EEPROM data corrupted, resetting to defaults.");
        resetConfiguration(potChannels);
    } else {
        Serial.println("Configuration loaded successfully!");
    }
}

uint8_t ConfigManager::getPotChannel(uint8_t potIndex) const {
    return _potChannels.at(potIndex);
}

uint8_t ConfigManager::getPotCCNumber(uint8_t potIndex) const {
    return _potCCNumbers.at(potIndex);
}

void ConfigManager::setPotChannel(uint8_t potIndex, uint8_t channel) {
    _potChannels[potIndex] = channel;
}

void ConfigManager::setPotCCNumber(uint8_t potIndex, uint8_t ccNumber) {
    _potCCNumbers[potIndex] = ccNumber;
}

void ConfigManager::saveConfiguration() {
    writeEEPROM();
}

bool ConfigManager::loadConfiguration(std::vector<uint8_t>& potChannels) {
    Serial.println("Loading configuration from EEPROM...");

    if (EEPROM.read(0) != 0xAA) { // Check if EEPROM is initialized
        Serial.println("EEPROM is uninitialized or corrupted.");
        return false;
    }

    for (uint8_t i = 0; i < _numPots; i++) {
        int address = i * 2;
        _potChannels[i] = EEPROM.read(address);
        potChannels[i] = _potChannels[i];  // Sync with external vector

        Serial.print("Pot ");
        Serial.print(i);
        Serial.print(": CC=");
        Serial.println(potChannels[i]);
    }

    Serial.println("Configuration loaded successfully.");
    return true;
}


void ConfigManager::resetConfiguration(std::vector<uint8_t>& potChannels) {
    Serial.println("Resetting EEPROM to default values...");
    
    for (uint8_t i = 0; i < _numPots; ++i) {
        _potChannels[i] = 1;  // Default to channel 1
        _potCCNumbers[i] = i; // Default to CC number matching the pot index
        potChannels[i] = i;   // Also reset `potChannels` passed from main
    }
    
    saveConfiguration();
}

void ConfigManager::readEEPROM() {
    for (uint8_t i = 0; i < _numPots; ++i) {
        int address = i * 2;
        _potChannels[i] = EEPROM.read(address);
        _potCCNumbers[i] = EEPROM.read(address + 1);
    }
}

void ConfigManager::writeEEPROM() {
    for (uint8_t i = 0; i < _numPots; ++i) {
        int address = i * 2;
        EEPROM.update(address, _potChannels[i]);
        EEPROM.update(address + 1, _potCCNumbers[i]);
    }
}