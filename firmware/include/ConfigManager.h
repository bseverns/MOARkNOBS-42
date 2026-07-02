// Manages reading and writing of configuration data in EEPROM.
// Other managers query this class for slot definitions and LED settings.
// Initialised by firmware_main.cpp at startup.

#ifndef CONFIGMANAGER_H
#define CONFIGMANAGER_H

#include "Globals.h"
#include "LedMode.h"
#include "MIDITypes.h"
#include "PotentiometerManager.h"
#include "storage/StorageBackend.h"
#include <Arduino.h>
#include <EEPROM.h>
#include <map>
#include <vector>
#include <array>
#include <FastLED.h>

SlotARGConfig sanitizeSlotArg(const SlotARGConfig &config);

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
 * 4*NUM_POTS + 13 Active profile index             1
 * 4*NUM_POTS + 17 USB MIDI out flag/check          2
 * 200             Primary magic (0xABCD)           2
 * 202             Backup magic  (0xDCBA)           2
 * EEPROM_EF_BASELINES (204) EF baselines          EEPROM_EF_BASELINES_SIZE
 * EEPROM_EF_BASELINES + EEPROM_EF_BASELINES_SIZE  Buffer           EEPROM_BUFFER_SIZE
 * EEPROM_BACKUP_START (250) Backup copy of config starts     mirrors layout
 * EEPROM_CONFIG_TAIL + 0x000  Profile A block       256 bytes
 * EEPROM_CONFIG_TAIL + 0x100  Profile B block       256 bytes
 * EEPROM_CONFIG_TAIL + 0x200  Profile C block       256 bytes
 * EEPROM_CONFIG_TAIL + 0x300  Profile D block       256 bytes
 * EEPROM_PROFILE_SETTINGS_BASE                     Profile payloads (EF/LFO/arp/LED/midi)
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
inline constexpr uint16_t EEPROM_ACTIVE_PROFILE = EEPROM_CONFIG_CRC + 2;
inline constexpr uint16_t EEPROM_LED_MODE = EEPROM_ACTIVE_PROFILE + 1;
inline constexpr uint16_t EEPROM_EF_IDLE_FLOOR = EEPROM_LED_MODE + 1;
inline constexpr uint16_t EEPROM_EF_IDLE_FLOOR_CHECK = EEPROM_EF_IDLE_FLOOR + 1;
inline constexpr uint16_t EEPROM_USB_MIDI_OUT = EEPROM_EF_IDLE_FLOOR_CHECK + 1;
inline constexpr uint16_t EEPROM_USB_MIDI_OUT_CHECK = EEPROM_USB_MIDI_OUT + 1;

class EnvelopeFollower;

/**
 * Profile payload chunks used for multi-profile persistence.
 * These structs are packed so EEPROM writes stay compact and predictable.
 */
struct __attribute__((packed)) ProfileEfSettings {
    uint8_t mode = 0;              //!< EnvelopeFollower::EFMode
    uint8_t autoBaseline = 1;      //!< Auto-baseline flag (0/1)
    uint8_t autoGain = 1;          //!< Auto-gain flag (0/1)
    uint8_t gateThreshold = 16;    //!< Gate threshold (0-127)
    uint8_t gateHysteresis = 4;    //!< Gate hysteresis (0-127)
    uint8_t activityThreshold = 4; //!< Activity threshold (0-127)
    uint8_t gainTarget = 102;      //!< Auto-gain target (0-127)
    uint8_t destinationMode =
        static_cast<uint8_t>(EfDestinationMode::AddClamp); //!< EfDestinationMode value
    uint16_t attackMs = 5;                                 //!< Attack time (ms)
    uint16_t releaseMs = 20;                               //!< Release time (ms)
    uint16_t rmsWindowMs = 50;                             //!< RMS window (ms)
    uint16_t baselineTauMs = 2000;                         //!< Baseline time constant (ms)
    uint16_t gainTauMs = 3000;                             //!< Gain time constant (ms)
};

struct __attribute__((packed)) ProfileSlotSettings {
    uint8_t midiChannel = 1;   //!< Per-slot MIDI channel
    int8_t followerIndex = -1; //!< Saved EF follower assignment for this slot
    ProfileEfSettings ef{};    //!< Per-slot EF mode settings
};

struct __attribute__((packed)) ProfileArpSettings {
    uint8_t lengthTicks = 12; //!< Step length in MIDI ticks
    uint8_t shape = 0;        //!< Arpeggiator::Shape
    uint8_t swingPercent = 0; //!< Swing percent (0..80)
    uint8_t gatePercent = 50; //!< Gate percent (5..100)
    uint8_t octaveRange = 0;  //!< Extra octaves (0..3)
};

struct __attribute__((packed)) ProfileLedSettings {
    uint8_t brightness = 128; //!< Base LED brightness
    uint8_t r = 0;            //!< LED color R
    uint8_t g = 0;            //!< LED color G
    uint8_t b = 0;            //!< LED color B
};

struct __attribute__((packed)) ProfileClockSettings {
    float tappedBpm = 120.0f;    //!< Internal tapped tempo used when not following external clock
    uint8_t clockOutEnabled = 0; //!< MIDI clock output gate (0/1)
    uint8_t followExternalClock = 1; //!< External clock follow flag (0/1)
};

struct __attribute__((packed)) ProfileNoteDynamicsSettings {
    int8_t velocityShift = 0;        //!< -64..+63 note velocity offset
    uint8_t changeProbability = 100; //!< 0..100% note trigger probability
};

struct __attribute__((packed)) ProfileJitterSettings {
    float depth = 1.0f;      //!< 0..1 jitter depth
    float smoothness = 0.5f; //!< 0..1 jitter smoothness
};

struct __attribute__((packed)) ProfileLfoSettings {
    uint8_t shape = 0;        //!< LFOShape value
    float frequencyHz = 1.0f; //!< Free-run frequency
    float depth = 0.0f;       //!< LFO depth (0..1)
    uint8_t bipolar = 1;      //!< Bipolar flag (0/1)
    uint8_t syncEnabled = 0;  //!< Sync flag (0/1)
    uint8_t syncRatio = 0;    //!< LFOSyncRatio value
};

struct __attribute__((packed)) ProfileLfoRoute {
    uint8_t type = 0;       //!< LFOManager::Route::Type
    uint8_t lfoIndex = 0;   //!< LFO index
    float depth = 1.0f;     //!< Route depth
    uint8_t target = 0;     //!< LFOInternalTarget
    uint8_t channel = 1;    //!< MIDI channel
    uint8_t ccMsb = 0;      //!< CC MSB
    uint8_t ccLsb = 32;     //!< CC LSB
    int8_t amount = 100;    //!< Signed route amount (-100..100)
    uint8_t minValue = 0;   //!< Output floor for transport routes
    uint8_t maxValue = 127; //!< Output ceiling for transport routes
};

inline constexpr uint8_t PROFILE_LFO_COUNT = 2;
inline constexpr uint8_t PROFILE_MAX_ROUTES = 8;
inline constexpr uint16_t PROFILE_SETTINGS_VERSION = 0x0005;

struct __attribute__((packed)) ProfileData {
    uint16_t version = PROFILE_SETTINGS_VERSION;  //!< Profile payload version
    uint16_t crc = 0;                             //!< CRC over payload bytes
    uint8_t routeCount = 0;                       //!< Active route count
    ProfileArpSettings arp{};                     //!< Arp settings snapshot
    ProfileLedSettings led{};                     //!< LED brightness/color
    ProfileClockSettings clock{};                 //!< Clock source/output snapshot
    ProfileNoteDynamicsSettings noteDynamics{};   //!< Live note dynamics snapshot
    ProfileJitterSettings jitter{};               //!< Live jitter snapshot
    ProfileLfoSettings lfos[PROFILE_LFO_COUNT]{}; //!< LFO state snapshot
    ProfileLfoRoute routes[PROFILE_MAX_ROUTES]{}; //!< LFO route table
    ProfileSlotSettings slots[NUM_SLOTS]{};       //!< Per-slot MIDI/EF settings
};

static_assert(sizeof(ProfileData) <= EEPROM_PROFILE_SETTINGS_BLOCK_SIZE,
              "ProfileData exceeds the allotted EEPROM block size");

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

    /** Load the extended profile payload (EF/LFO/arp/LED/midi channel). */
    bool loadProfileSettings(uint8_t id, ProfileData &profile) const;

    /** Save the extended profile payload (EF/LFO/arp/LED/midi channel). */
    bool saveProfileSettings(uint8_t id, const ProfileData &profile);

    /** Return the last stored active profile index. */
    uint8_t getActiveProfile() const { return _stored.activeProfile; }

    /** Persist the active profile index to EEPROM. */
    void setActiveProfile(uint8_t id);

    // LED settings -------------------------------------------------------

    /** Persist LED brightness and colour to EEPROM. */
    void saveLEDSettings(uint8_t brightness, CRGB color);

    /** Retrieve LED brightness and colour from EEPROM. */
    void loadLEDSettings(uint8_t &brightness, CRGB &color);

    /** Persist which animation mode the LEDs should run in. */
    void setLedMode(LedMode mode);

    /** Retrieve the currently stored LED animation mode. */
    LedMode getLedMode() const;

    /** Persist the global EF idle/noise-floor clamp in MIDI units (0-127). */
    void setEfIdleFloor(uint8_t floor);

    /** Retrieve the current global EF idle/noise-floor clamp. */
    uint8_t getEfIdleFloor() const;

    /** Persist and apply the USB MIDI output gate. */
    void setUsbMidiOutEnabled(bool enabled);

    /** Override the persistence backend used by ConfigManager (tests/migration hooks). */
    static void setStorageBackend(StorageBackend *backend);

    /** Return the currently active persistence backend. */
    static StorageBackend *getStorageBackend();

    enum class RecoveryEvent {
        kNone,
        kDefaultsLoaded,
        kBackupRestored,
    };

    enum class LoadSource {
        kUnknown,
        kPrimary,
        kBackup,
        kDefaults,
    };

    /** Reset configuration to factory defaults. */
    void resetConfiguration(std::vector<uint8_t> &potChannels, bool recordRecoveryEvent = false);

    /** Consume the latest recovery event (backup or defaults). */
    RecoveryEvent consumeRecoveryEvent();

    /** Report which EEPROM path produced the currently loaded configuration. */
    LoadSource getLastLoadSource() const { return _lastLoadSource; }

    /** Check whether the primary or backup configuration header is currently valid. */
    bool hasHealthyConfigurationCopy(bool backup, uint16_t base = EEPROM_PROFILE_START(0)) const;

    // Envelope follower configuration -----------------------------------

    /** Persist the current envelope routing and baselines to EEPROM. */
    void saveEnvelopeSettings(const std::map<int, MIDISlot::EfSettings> &potToEnvelopeMap,
                              const std::vector<EnvelopeFollower> &envelopes);

    /**
     * Restore envelope routing and baseline offsets from EEPROM.
     * Returns true if every envelope's baseline was recovered.
     */
    bool loadEnvelopeSettings(std::map<int, MIDISlot::EfSettings> &potToEnvelopeMap,
                              std::vector<EnvelopeFollower> &envelopes);

    /** Save a single envelope follower's baseline to EEPROM. */
    void saveEnvelopeBaseline(uint8_t envIndex, float baseline);

    // Utility methods ----------------------------------------------------
    uint8_t getNumPots() const { return _numPots; }
    uint8_t getNumButtons() const { return _numButtons; }

    /** Store whether the system is in SEF or ARG envelope mode (legacy compatibility). */
    void setMode(uint8_t mode);

    /** Retrieve the stored envelope follower mode (legacy compatibility). */
    uint8_t getMode() const;

    /** Save which ARG combination method is currently selected (legacy compatibility). */
    void setARGMethod(uint8_t method);

    /** Retrieve the stored ARG method (legacy compatibility). */
    uint8_t getARGMethod() const;

    /** Flip the legacy ARG engine on or off. */
    void setARGEnable(uint8_t enable);

    /** Retrieve whether the legacy ARG path is currently enabled. */
    uint8_t getARGEnable() const;

    /** Persist which two envelope inputs are used for the legacy ARG calculations. */
    void setEnvelopePair(uint8_t envA, uint8_t envB);

    /** Retrieve the first envelope pin used by the legacy ARG tuple. */
    uint8_t getEnvelopeA() const;

    /** Retrieve the second envelope pin used by the legacy ARG tuple. */
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

    /** Fetch the stored envelope payload associated with a slot. */
    SlotEnvelopePayload getSlotEnvelopePayload(uint8_t idx) const;

    /** Replace the stored envelope payload for a slot and persist it. */
    void setSlotEnvelopePayload(uint8_t idx, const SlotEnvelopePayload &payload);

    /** Mirror the most recent global filter tuning into the EEPROM tail. */
    /**
     * Persist the legacy global filter tail settings. This is a fallback value
     * and does not override active slot-specific filter configurations.
     */
    SlotEnvelopePayload persistFilterTail(const SlotEnvelopePayload &payload);

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
    uint16_t calculateCRC(
        bool includeProfile) const; // compute config CRC (optionally include active profile)

    struct StoredConfig {
        std::array<uint8_t, NUM_POTS> potChannels;  //!< saved pot→channel map
        std::array<uint8_t, NUM_POTS> potCCNumbers; //!< saved CC numbers
        uint8_t activeProfile = 0;                  //!< stored active profile index
        uint8_t ledMode = static_cast<uint8_t>(LedMode::Static);
        uint16_t version = 0; //!< config schema version
        uint16_t crc = 0;     //!< integrity check value
    } _stored;

    void sanitizeSlotArena();
    void wipeSlotRegion();
    void wipeProfileBlocks();
    static bool slotLooksSane(const MIDISlot &candidate);
    void migrateLegacySlotPayloads(uint16_t storedVersion);
    SlotEnvelopePayload seedSlotEnvelopePayloads(uint8_t filterType, float freq, float q);
    static SlotEnvelopePayload sanitizeEnvelopePayload(const SlotEnvelopePayload &payload);
    struct LegacyARGSettings {
        uint8_t mode = 0;
        uint8_t method = 0;
        uint8_t enable = 0;
        uint8_t sourceA = 0;
        uint8_t sourceB = 1;
    } legacyArg{};
    RecoveryEvent _lastRecoveryEvent = RecoveryEvent::kNone;
    LoadSource _lastLoadSource = LoadSource::kUnknown;
    void loadLegacyARGSettings();
    void migrateLegacyARGSettings();
};

#endif // CONFIGMANAGER_H
