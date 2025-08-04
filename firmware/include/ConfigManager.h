// Manages reading and writing of configuration data in EEPROM.
// Other managers query this class for slot definitions and LED settings.
// Initialised by firmware_main.cpp at startup.

#ifndef CONFIGMANAGER_H
#define CONFIGMANAGER_H

#include "Globals.h"
#include "MIDITypes.h"
#include "PotentiometerManager.h"
#include <Arduino.h>
#include <EEPROM.h>
#include <map>
#include <vector>
#include <array>
#include <FastLED.h>

class MIDIHandler;

#ifndef DOXYGEN_SHOULD_SKIP_THIS
/*
 * ------------- EEPROM Memory Map -------------
 * Offset (bytes)  Region                             Bytes
 * --------------------------------------------------------
 * 0               Pot channel map                  NUM_POTS
 * NUM_POTS        Pot CC map                       NUM_POTS
 * 2*NUM_POTS      Envelope assignments             NUM_POTS
 * 3*NUM_POTS      Envelope types                   NUM_POTS
 * 4*NUM_POTS      LED brightness                   1
 * 4*NUM_POTS + 1  LED colour (RGB)                 3
 * 4*NUM_POTS + 4  ARG mode                         1
 * 4*NUM_POTS + 5  ARG method                       1
 * 4*NUM_POTS + 6  ARG env A                        1
 * 4*NUM_POTS + 7  ARG env B                        1
 * 4*NUM_POTS + 8  Config version                   2
 * 4*NUM_POTS + 10 CRC                              2
 * 4*NUM_POTS + 12 Envelope baselines               6 * 4
 * 4*NUM_POTS + 36-225 Reserved/buffer                 ~22
 * 200             Primary magic (0xABCD)           2
 * 202             Backup magic  (0xDCBA)           2
 * 204-225        Reserved/buffer                 part of above
 * 226             Backup copy of config starts     mirrors layout
 * 256             Profile 1 block begins           256 bytes
 * 512             Profile 2 block begins           256 bytes
 * --------------------------------------------------------
 * Backup strategy: Write the primary block and tag it with
 * EEPROM_MAGIC_PRIMARY. If the post-write check flops, the
 * firmware punts to the backup block (tagged with
 * EEPROM_MAGIC_BACKUP) as a safety net. On boot the tags are
 * inspected in order-primary first, backup second-to decide
 * which copy to trust. Each additional profile repeats this
 * layout in its own 256-byte slice.
 */
#endif

#define EEPROM_PROFILE_BLOCK_SIZE 256
#define EEPROM_PROFILE_START(id) (EEPROM_PROFILE_BLOCK_SIZE * (id))

#define EEPROM_START_ADDRESS 0
#define EEPROM_MAGIC_ADDRESS (EEPROM_START_ADDRESS + 200)  // Reserve space for config + magic number
#define EEPROM_MAGIC_PRIMARY 0xABCD  // Validates the main config block
#define EEPROM_MAGIC_BACKUP  0xDCBA  // Signals a sane backup image

#define CONFIG_VERSION 0x0001

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
#define EEPROM_CONFIG_VERSION (EEPROM_ARG_ENV_B + 1)
#define EEPROM_CONFIG_CRC    (EEPROM_CONFIG_VERSION + 2)
#define EEPROM_ENVELOPE_BASELINES (EEPROM_CONFIG_CRC + 2)
#define EEPROM_BACKUP_START  (EEPROM_CONFIG_CRC + 2 + 46)  // Space after primary + buffer

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
    bool loadConfiguration(std::vector<uint8_t>& potChannels, uint16_t base = EEPROM_PROFILE_START(0));

    /** Load a saved profile from one of the reserved EEPROM blocks. */
    void loadProfile(uint8_t id);

    /** Save the current in-RAM settings to a profile block. */
    void saveProfile(uint8_t id);

    // LED settings -------------------------------------------------------

    /** Persist LED brightness and colour to EEPROM. */
    void saveLEDSettings(uint8_t brightness, CRGB color);

    /** Retrieve LED brightness and colour from EEPROM. */
    void loadLEDSettings(uint8_t& brightness, CRGB& color);

    /** Reset configuration to factory defaults. */
    void resetConfiguration(std::vector<uint8_t>& potChannels);

    // Envelope follower configuration -----------------------------------

    /** Persist the current envelope routing and baselines to EEPROM. */
    void saveEnvelopeSettings(const std::map<int, int>& potToEnvelopeMap, const std::vector<EnvelopeFollower>& envelopes);

    /**
     * Restore envelope routing and baseline offsets from EEPROM.
     * Returns true if every envelope's baseline was recovered.
     */
    bool loadEnvelopeSettings(std::map<int, int>& potToEnvelopeMap, std::vector<EnvelopeFollower>& envelopes);

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

    /**
     * Parse a WebSerial command. Returns true if the command was
     * recognised and handled.
     */
    bool handleCommand(const String& command);

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
    bool checkEEPROMHealth(bool backup, uint16_t base = EEPROM_PROFILE_START(0)); // verify header magic
    void writeMagicNumber(bool backup, uint16_t base = EEPROM_PROFILE_START(0));  // stamp EEPROM with magic
    bool loadBackupConfiguration(std::vector<uint8_t>& potChannels, uint16_t base); // restore from backup copy
    void readEEPROM(bool backup, uint16_t base);        // raw EEPROM read helper
    void writeEEPROM(bool backup, uint16_t base);       // raw EEPROM write helper
    uint16_t calculateCRC() const;                      // compute config CRC

    //virtual slot/array:
    std::array<uint8_t, NUM_POTS>   _potChannels; // your pot→CC map
    std::array<uint8_t, NUM_POTS>   _potCCNumbers;
    uint16_t _storedVersion = 0;                      // version read from EEPROM
    uint16_t _storedCRC = 0;                          // CRC read from EEPROM
};

#endif // CONFIGMANAGER_H
