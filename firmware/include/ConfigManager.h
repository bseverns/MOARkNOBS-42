#ifndef CONFIGURATION_MANAGER_H
#define CONFIGURATION_MANAGER_H

#include <Arduino.h>
#include <EEPROM.h>
#include <Globals.h>
#include <map>
#include <vector>
#include <FastLED.h>
#include <EnvelopeFollower.h>

#define EEPROM_LED_BRIGHTNESS (EEPROM_ENVELOPE_TYPES + NUM_POTS)
#define EEPROM_LED_COLOR (EEPROM_LED_BRIGHTNESS + 1)
#define EEPROM_START_ADDRESS 0  // Define EEPROM storage start address
#define EEPROM_POT_CHANNELS EEPROM_START_ADDRESS
#define EEPROM_POT_CC (EEPROM_POT_CHANNELS + NUM_POTS)  // Offset after channels
#define EEPROM_ENVELOPE_ASSIGNMENTS (EEPROM_POT_CC + NUM_POTS)
#define EEPROM_ENVELOPE_TYPES (EEPROM_ENVELOPE_ASSIGNMENTS + NUM_POTS)
#define EEPROM_ARG_MODE     (EEPROM_LED_COLOR + 3)
#define EEPROM_ARG_METHOD   (EEPROM_ARG_MODE + 1)
#define EEPROM_ARG_ENV_A    (EEPROM_ARG_METHOD + 1)
#define EEPROM_ARG_ENV_B    (EEPROM_ARG_ENV_A + 1)

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

    // LED EEPROM management
    void saveLEDSettings(uint8_t brightness, CRGB color);
    void loadLEDSettings(uint8_t& brightness, CRGB& color);

    // Reset configuration to defaults
    void resetConfiguration(std::vector<uint8_t>& potChannels);

    // Envelope Follower configuration
    void saveEnvelopeSettings(const std::map<int, int>& potToEnvelopeMap, const std::vector<EnvelopeFollower>& envelopes);
    void loadEnvelopeSettings(std::map<int, int>& potToEnvelopeMap, std::vector<EnvelopeFollower>& envelopes);

    // Utility method to get global constants
    uint8_t getNumPots() const { return _numPots; }
    uint8_t getNumButtons() const { return _numButtons; }

    // Store and load the Envelope Follower mode (SEF or ARG)
    void setMode(uint8_t mode);     // 0 = SEF, 1 = ARG, etc.
    uint8_t getMode() const;

    // Store and load the ARG method (PLUS, MIN, PECK, etc.)
    void setARGMethod(uint8_t method);
    uint8_t getARGMethod() const;

    // Store and load the two envelope “pins” used in ARG mode
    void setEnvelopePair(uint8_t envA, uint8_t envB);
    uint8_t getEnvelopeA() const;
    uint8_t getEnvelopeB() const;

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