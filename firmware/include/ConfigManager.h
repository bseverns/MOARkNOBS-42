#ifndef CONFIGURATION_MANAGER_H
#define CONFIGURATION_MANAGER_H

#include <Arduino.h>
#include <EEPROM.h>
#include <map>
#include <string>

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


    // Reset configuration to defaults
    void resetConfiguration(std::vector<uint8_t>& potChannels);

    // Utility method to get global constants
    uint8_t getNumPots() const { return _numPots; }
    uint8_t getNumButtons() const { return _numButtons; }

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