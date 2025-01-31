#ifndef CONFIGURATION_MANAGER_H
#define CONFIGURATION_MANAGER_H

#include <Arduino.h>
#include <EEPROM.h>
#include <Globals.h>
#include <map>
#include <string>
#include <FastLED.h>
#include <EnvelopeFollower.h>

#define EEPROM_LED_BRIGHTNESS (EEPROM_ENVELOPE_TYPES + NUM_POTS)
#define EEPROM_LED_COLOR (EEPROM_LED_BRIGHTNESS + 1)
#define EEPROM_START_ADDRESS 0  // Define EEPROM storage start address
#define EEPROM_POT_CHANNELS EEPROM_START_ADDRESS
#define EEPROM_POT_CC (EEPROM_POT_CHANNELS + NUM_POTS)  // Offset after channels
#define EEPROM_ENVELOPE_ASSIGNMENTS (EEPROM_POT_CC + NUM_POTS)
#define EEPROM_ENVELOPE_TYPES (EEPROM_ENVELOPE_ASSIGNMENTS + NUM_POTS)

class EnvelopeFollower;

class ConfigManager {
public:
    ConfigManager(uint8_t numPots, uint8_t numButtons);

    // Initialize configuration (e.g., load from EEPROM)
    void begin(std::vector<uint8_t>& potChannels);

    // Accessor methods for key configurations
    uint8_t getPotChannel(uint8_t potIndex) const;
    uint8_t getPotCCNumber(uint8_t potIndex) const;
    void setPotChannel(uint8_t potIndex, uint8_t channel);
    void setPotCCNumber(uint8_t potIndex, uint8_t ccNumber);

    // Save and load configurations from EEPROM
    void saveConfiguration();
    bool loadConfiguration(std::vector<uint8_t>& potChannels);

    //LED EEPROM
    void saveLEDSettings(uint8_t brightness, CRGB color);
    void loadLEDSettings(uint8_t& brightness, CRGB& color);

    // Reset configuration to defaults
    void resetConfiguration(std::vector<uint8_t>& potChannels);

    // Utility method to get global constants
    uint8_t getNumPots() const { return _numPots; }
    uint8_t getNumButtons() const { return _numButtons; }

    void saveEnvelopeSettings(const std::map<int, int>& potToEnvelopeMap, const std::vector<EnvelopeFollower>& envelopes);
    void loadEnvelopeSettings(std::map<int, int>& potToEnvelopeMap, std::vector<EnvelopeFollower>& envelopes);


private:
    uint8_t _numPots;
    uint8_t _numButtons;

    // Configuration data (stored in RAM)
    std::map<uint8_t, uint8_t> _potChannels;   // Potentiometer index -> MIDI Channel
    std::map<uint8_t, uint8_t> _potCCNumbers; // Potentiometer index -> MIDI CC Number

    // Internal helper methods for EEPROM operations
    void readEEPROM();
    void writeEEPROM();
};

#endif // CONFIGURATION_MANAGER_H