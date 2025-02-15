#include "ConfigManager.h"

// Constructor
ConfigManager::ConfigManager(uint8_t numPots, uint8_t numButtons)
    : _numPots(numPots), _numButtons(numButtons) {}

// Save the configuration to persistent storage (e.g., EEPROM)
void ConfigManager::saveConfiguration() {
    for (uint8_t i = 0; i < _numPots; i++) {
        EEPROM.update(EEPROM_POT_CHANNELS + i, _potChannels[i]);
        EEPROM.update(EEPROM_POT_CC + i, _potCCNumbers[i]);
    }
}

// Load the configuration from persistent storage
bool ConfigManager::loadConfiguration(std::vector<uint8_t>& potChannels) {
    potChannels.clear();
    for (uint8_t i = 0; i < _numPots; i++) {
        uint8_t channel = EEPROM.read(EEPROM_POT_CHANNELS + i);
        uint8_t ccNumber = EEPROM.read(EEPROM_POT_CC + i);
        _potChannels[i] = channel;
        _potCCNumbers[i] = ccNumber;
        potChannels.push_back(channel);
    }
    return true; // Indicate successful load
}

// Initialize the configuration
void ConfigManager::begin(std::vector<uint8_t>& potChannels) {
    loadConfiguration(potChannels); // Load settings from EEPROM
}

// Set the MIDI channel for a specific pot
void ConfigManager::setPotChannel(uint8_t potIndex, uint8_t channel) {
    if (potIndex < _numPots) {
        _potChannels[potIndex] = channel;
    }
}

// Set the MIDI CC number for a specific pot
void ConfigManager::setPotCCNumber(uint8_t potIndex, uint8_t ccNumber) {
    if (potIndex < _numPots) {
        _potCCNumbers[potIndex] = ccNumber;
    }
}

// Get the MIDI channel for a specific pot
uint8_t ConfigManager::getPotChannel(uint8_t potIndex) const {
    return _potChannels.at(potIndex);
}

// Get the MIDI CC number for a specific pot
uint8_t ConfigManager::getPotCCNumber(uint8_t potIndex) const {
    return _potCCNumbers.at(potIndex);
}

// Load Envelope Follower settings
void ConfigManager::loadEnvelopeSettings(std::map<int, int>& potToEnvelopeMap, std::vector<EnvelopeFollower>& envelopeFollowers) {
    for (size_t i = 0; i < envelopeFollowers.size(); i++) {
        int envelopeIndex = EEPROM.read(EEPROM_ENVELOPE_ASSIGNMENTS + i);
        potToEnvelopeMap[i] = envelopeIndex;
    }
}

// Save Envelope Follower settings
void ConfigManager::saveEnvelopeSettings(const std::map<int, int>& potToEnvelopeMap, const std::vector<EnvelopeFollower>& envelopes) {
    for (const auto& [potIndex, envelopeIndex] : potToEnvelopeMap) {
        EEPROM.update(EEPROM_ENVELOPE_ASSIGNMENTS + potIndex, envelopeIndex);
    }
}

// Load LED settings
void ConfigManager::loadLEDSettings(uint8_t& brightness, CRGB& color) {
    brightness = EEPROM.read(EEPROM_LED_BRIGHTNESS);
    color.r = EEPROM.read(EEPROM_LED_COLOR);
    color.g = EEPROM.read(EEPROM_LED_COLOR + 1);
    color.b = EEPROM.read(EEPROM_LED_COLOR + 2);
}

// Save LED settings
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
