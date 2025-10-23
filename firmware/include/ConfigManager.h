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
 * See `docs/EEPROMLayout.md` for a table version.
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
 * 4*NUM_POTS + 8  ARG enable                       1
 * 4*NUM_POTS + 9  Config version                   2
 * 4*NUM_POTS + 11 CRC                              2
 * 200             Primary magic (0xABCD)           2
 * 202             Backup magic  (0xDCBA)           2
 * EEPROM_EF_BASELINES (204) EF baselines          EEPROM_EF_BASELINES_SIZE
 * EEPROM_EF_BASELINES + EEPROM_EF_BASELINES_SIZE  Buffer           EEPROM_BUFFER_SIZE
 * EEPROM_BACKUP_START (250) Backup copy of config starts     mirrors layout
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

inline constexpr uint16_t EEPROM_POT_CHANNELS = EEPROM_START_ADDRESS;
inline constexpr uint16_t EEPROM_POT_CC = EEPROM_POT_CHANNELS + NUM_POTS;
inline constexpr uint16_t EEPROM_ENVELOPE_ASSIGNMENTS = EEPROM_POT_CC + NUM_POTS;
inline constexpr uint16_t EEPROM_ENVELOPE_TYPES = EEPROM_ENVELOPE_ASSIGNMENTS + NUM_POTS;
inline constexpr uint16_t EEPROM_LED_BRIGHTNESS = EEPROM_ENVELOPE_TYPES + NUM_POTS;
inline constexpr uint16_t EEPROM_LED_COLOR = EEPROM_LED_BRIGHTNESS + 1;
inline constexpr uint16_t EEPROM_ARG_MODE = EEPROM_LED_COLOR + 3;
inline constexpr uint16_t EEPROM_ARG_METHOD = EEPROM_ARG_MODE + 1;
inline constexpr uint16_t EEPROM_ARG_ENV_A = EEPROM_ARG_METHOD + 1;
inline constexpr uint16_t EEPROM_ARG_ENV_B = EEPROM_ARG_ENV_A + 1;
inline constexpr uint16_t EEPROM_ARG_ENABLE = EEPROM_ARG_ENV_B + 1;
inline constexpr uint16_t EEPROM_CONFIG_VERSION = EEPROM_ARG_ENABLE + 1;
inline constexpr uint16_t EEPROM_CONFIG_CRC = EEPROM_CONFIG_VERSION + 2;

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
     * potChannels with the stored pot→MIDI-channel map.
     */
    void begin(std::vector<uint8_t> &potChannels);

    MIDIMessageType getSlotType(uint8_t idx) const { return slots[idx].type; }
    void setSlotType(uint8_t idx, MIDIMessageType t) {
        slots[idx].type = t;
        saveSlot(idx, slots[idx]);
    }
    bool getSlotActive(uint8_t idx) const { return slots[idx].active; }
    void setSlotActive(uint8_t idx, bool a) {
        slots[idx].active = a;
        saveSlot(idx, slots[idx]);
    }
    uint8_t getSlotData1(uint8_t idx) const { return slots[idx].data1; }
    void setSlotData1(uint8_t idx, uint8_t v) {
        slots[idx].data1 = v;
        saveSlot(idx, slots[idx]);
    }

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

    /**
     * Load configuration from EEPROM. Returns false if data was invalid.
     * Fills potChannels with the persisted pot→channel map.
     */
    bool loadConfiguration(std::vector<uint8_t> &potChannels,
                           uint16_t base = EEPROM_PROFILE_START(0));

    /** Load a saved profile from one of the reserved EEPROM blocks. */
    void loadProfile(uint8_t id);

    /** Save the current in-RAM settings to a profile block. */
    void saveProfile(uint8_t id);

    // LED settings -------------------------------------------------------

    /** Persist LED brightness and colour to EEPROM. */
    void saveLEDSettings(uint8_t brightness, CRGB color);

    /** Retrieve LED brightness and colour from EEPROM. */
    void loadLEDSettings(uint8_t &brightness, CRGB &color);

    /** Reset configuration to factory defaults. */
    void resetConfiguration(std::vector<uint8_t> &potChannels);

    // Envelope follower configuration -----------------------------------

    /** Persist the current envelope routing and baselines to EEPROM. */
    void saveEnvelopeSettings(const std::map<int, int> &potToEnvelopeMap,
                              const std::vector<EnvelopeFollower> &envelopes);

    /**
     * Restore envelope routing and baseline offsets from EEPROM.
     * Returns true if every envelope's baseline was recovered.
     */
    bool loadEnvelopeSettings(std::map<int, int> &potToEnvelopeMap,
                              std::vector<EnvelopeFollower> &envelopes);

    /** Save a single envelope follower's baseline to EEPROM. */
    void saveEnvelopeBaseline(uint8_t envIndex, float baseline);

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

    /** Flip the ARG engine on or off. */
    void setARGEnable(uint8_t enable);

    /** Retrieve whether ARG is currently enabled. */
    uint8_t getARGEnable() const;

    /** Persist which two envelope inputs are used for ARG calculations. */
    void setEnvelopePair(uint8_t envA, uint8_t envB);

    /** Retrieve the first envelope index used by ARG mode. */
    uint8_t getEnvelopeA() const;

    /** Retrieve the second envelope index used by ARG mode. */
    uint8_t getEnvelopeB() const;

    // MIDI slot configuration -------------------------------------------

    /** Save an array of MIDISlot structures to EEPROM. */
    void saveMIDISlots(const MIDISlot *slots, size_t count);

    /** Load MIDISlot structures from EEPROM into the provided buffer. */
    void loadMIDISlots(MIDISlot *slots, size_t count);

    /**
     * Parse a WebSerial command. Returns true if the command was
     * recognised and handled.
     */
    bool handleCommand(const String &command);

    /** Determine if the display should switch to the screensaver. */
    bool shouldRunScreensaver() const;

    /** Execute the idle screensaver animation. */
    void runIdleScreensaver();

    /** Accessor so the rest of your code can see the live slots. */
    const std::array<MIDISlot, NUM_SLOTS> &getSlots() const { return slots; }

    /** Return a reference to a specific slot. */
    MIDISlot &getSlot(uint8_t idx) { return slots[idx]; }

    /** Read a single MIDISlot from EEPROM into the provided struct. */
    void loadSlot(uint8_t idx, MIDISlot &dest);
    /** Write one MIDISlot structure back to EEPROM. */
    void saveSlot(uint8_t idx, const MIDISlot &src);

  private:
    uint8_t _numPots;
    uint8_t _numButtons;

    std::array<MIDISlot, NUM_SLOTS> slots; // 42 of them

    // Health‑check & backup support
    bool checkEEPROMHealth(bool backup,
                           uint16_t base = EEPROM_PROFILE_START(0)); // verify header magic
    void writeMagicNumber(bool backup,
                          uint16_t base = EEPROM_PROFILE_START(0)); // stamp EEPROM with magic
    bool loadBackupConfiguration(std::vector<uint8_t> &potChannels,
                                 uint16_t base);  // restore from backup copy
    void readEEPROM(bool backup, uint16_t base);  // raw EEPROM read helper
    void writeEEPROM(bool backup, uint16_t base); // raw EEPROM write helper
    uint16_t calculateCRC() const;                // compute config CRC

    struct StoredConfig {
        std::array<uint8_t, NUM_POTS> potChannels;  //!< saved pot→channel map
        std::array<uint8_t, NUM_POTS> potCCNumbers; //!< saved CC numbers
        uint16_t version = 0;                       //!< config schema version
        uint16_t crc = 0;                           //!< integrity check value
    } _stored;

    void sanitizeSlotArena();
    void wipeSlotRegion();
    void wipeProfileBlocks();
    static bool slotLooksSane(const MIDISlot &candidate);
};

#endif // CONFIGMANAGER_H
