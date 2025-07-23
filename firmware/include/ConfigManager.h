#ifndef CONFIGMANAGER_H
#define CONFIGMANAGER_H

#include "Globals.h"
#include "MIDITypes.h"
#include <Arduino.h>
#include <EEPROM.h>
#include <map>
#include <vector>
#include <array>
#include <FastLED.h>

class MIDIHandler;
extern MIDIHandler midihandler;

#define EEPROM_START_ADDRESS 0
#define EEPROM_MAGIC_ADDRESS (EEPROM_START_ADDRESS + 200)  // Reserve space for config + magic number
#define EEPROM_MAGIC_PRIMARY 0xABCD
#define EEPROM_MAGIC_BACKUP  0xDCBA

#define EEPROM_POT_CHANNELS EEPROM_START_ADDRESS
#define EEPROM_POT_CC (EEPROM_POT_CHANNELS + NUM_POTS)
#define EEPROM_ENVELOPE_ASSIGNMENTS (EEPROM_POT_CC + NUM_POTS)
#define EEPROM_ENVELOPE_TYPES (EEPROM_ENVELOPE_ASSIGNMENTS + NUM_POTS)
#define EEPROM_LED_BRIGHTNESS (EEPROM_ENVELOPE_TYPES + NUM_POTS)
#define EEPROM_LED_COLOR (EEPROM_LED_BRIGHTNESS + 1)
#define EEPROM_ARG_MODE     (EEPROM_LED_COLOR + 3)
#define EEPROM_ARG_METHOD   (EEPROM_ARG_MODE + 1)
#define EEPROM_ARG_ENV_A    (EEPROM_ARG_METHOD + 1)
#define EEPROM_ARG_ENV_B    (EEPROM_ARG_ENV_A + 1)
#define EEPROM_BACKUP_START (EEPROM_ARG_ENV_B + 1 + 50)  // Space after primary + buffer

class EnvelopeFollower;

/**
 * @brief Handles persistence of user configuration in EEPROM.
 *
 * ConfigManager provides helpers for saving and loading all
 * user‑modifiable settings such as pot assignments, LED configuration
 * and MIDI slot definitions.  It also maintains a backup area in
 * EEPROM and performs simple integrity checks using a magic number.
 */
class ConfigManager {
public:
    /**
     * Create a configuration manager for the given number of pots and
     * buttons. No EEPROM access occurs here.
     */
    ConfigManager(uint8_t numPots, uint8_t numButtons);

    /** Generate the JSON configuration schema string. */
    static String makeSchema();

    /** Serialize the entire configuration to a JSON-like string. */
    String serializeAll() const;

    /**
     * Initialise the subsystem and load settings from EEPROM. Populates
     * potChannels with the stored pot→CC map.
     */
    void begin(std::vector<uint8_t>& potChannels);

    MIDIMessageType getSlotType(uint8_t idx) const { return slots[idx].type; }
    void setSlotType(uint8_t idx, MIDIMessageType t) { slots[idx].type = t; saveSlot(idx, slots[idx]); }
    bool getSlotActive(uint8_t idx) const { return slots[idx].active; }
    void setSlotActive(uint8_t idx, bool a) { slots[idx].active = a; saveSlot(idx, slots[idx]); }
    uint8_t getSlotData1(uint8_t idx) const { return slots[idx].data1; }
    void    setSlotData1(uint8_t idx, uint8_t v){ slots[idx].data1=v; saveSlot(idx,slots[idx]); }

    // Accessors -----------------------------------------------------------

    /** Retrieve the stored MIDI channel for the given pot. */
    uint8_t getPotChannel(uint8_t potIndex) const;

    /** Retrieve the stored CC number for the given pot. */
    uint8_t getPotCCNumber(uint8_t potIndex) const;

    /** Assign a MIDI channel to a pot. */
    void setPotChannel(uint8_t potIndex, uint8_t channel);

    /** Assign a CC number to a pot. */
    void setPotCCNumber(uint8_t potIndex, uint8_t ccNumber);

    // Persistence --------------------------------------------------------

    /** Write all configuration data to EEPROM with backup verification. */
    void saveConfiguration();

    /** Load configuration from EEPROM. Returns false if data was invalid. */
    bool loadConfiguration(std::vector<uint8_t>& potChannels);

    // LED settings -------------------------------------------------------

    /** Persist LED brightness and colour to EEPROM. */
    void saveLEDSettings(uint8_t brightness, CRGB color);

    /** Retrieve LED brightness and colour from EEPROM. */
    void loadLEDSettings(uint8_t& brightness, CRGB& color);

    /** Reset configuration to factory defaults. */
    void resetConfiguration(std::vector<uint8_t>& potChannels);

    // Envelope follower configuration -----------------------------------

    /** Persist the current envelope routing and types to EEPROM. */
    void saveEnvelopeSettings(const std::map<int, int>& potToEnvelopeMap, const std::vector<EnvelopeFollower>& envelopes);

    /** Restore envelope routing and filter types from EEPROM. */
    void loadEnvelopeSettings(std::map<int, int>& potToEnvelopeMap, std::vector<EnvelopeFollower>& envelopes);

    // Utility methods ----------------------------------------------------
    uint8_t getNumPots() const { return _numPots; }
    uint8_t getNumButtons() const { return _numButtons; }

    /** Store whether the system is in SEF or ARG envelope mode. */
    void setMode(uint8_t mode);

    /** Retrieve the stored envelope follower mode. */
    uint8_t getMode() const;

    /** Save which ARG combination method is currently selected. */
    void setARGMethod(uint8_t method);

    /** Retrieve the stored ARG method. */
    uint8_t getARGMethod() const;

    /** Persist which two envelope inputs are used for ARG calculations. */
    void setEnvelopePair(uint8_t envA, uint8_t envB);

    /** Retrieve the first envelope index used by ARG mode. */
    uint8_t getEnvelopeA() const;

    /** Retrieve the second envelope index used by ARG mode. */
    uint8_t getEnvelopeB() const;

    // MIDI slot configuration -------------------------------------------

    /** Save an array of MIDISlot structures to EEPROM. */
    void saveMIDISlots(const MIDISlot* slots, size_t count);

    /** Load MIDISlot structures from EEPROM into the provided buffer. */
    void loadMIDISlots(MIDISlot* slots, size_t count);

    /** Determine if the display should switch to the screensaver. */
    bool shouldRunScreensaver() const;

    /** Execute the idle screensaver animation. */
    void runIdleScreensaver();

    /** Accessor so the rest of your code can see the live slots. */
    const std::array<MIDISlot,NUM_SLOTS>& getSlots() const { return slots; }

    /** Return a reference to a specific slot. */
    MIDISlot& getSlot(uint8_t idx){
         return slots[idx];
    }

    /** Read a single MIDISlot from EEPROM into the provided struct. */
    void loadSlot(uint8_t idx, MIDISlot& dest);
    /** Write one MIDISlot structure back to EEPROM. */
    void saveSlot(uint8_t idx, const MIDISlot& src);

private:
    uint8_t _numPots;
    uint8_t _numButtons;

    std::array<MIDISlot,NUM_SLOTS> slots;//42 of them

    // Health‑check & backup support
    bool checkEEPROMHealth(bool backup);               // verify header magic
    void writeMagicNumber(bool backup);                // stamp EEPROM with magic
    bool loadBackupConfiguration(std::vector<uint8_t>& potChannels); // restore from backup copy
    void readEEPROM(bool backup);                      // raw EEPROM read helper
    void writeEEPROM(bool backup);                     // raw EEPROM write helper

    //virtual slot/array:
    std::array<uint8_t, NUM_POTS>   _potChannels; // your pot→CC map
    std::array<uint8_t, NUM_POTS>   _potCCNumbers;
};

#endif // CONFIGMANAGER_H
