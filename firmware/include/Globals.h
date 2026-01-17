/**
 * Global juice for the MOAR kNOBS firmware.
 *
 * LED gangs:
 *   - `SLOT_LED_COUNT`, `EF_LED_COUNT`, and `POT_LED_COUNT` define the three
 *     clans of pixels on the strip.  Offsets like `EF_LED_OFFSET` and
 *     `POT_LED_OFFSET` stitch them together into one loud `NUM_LEDS` parade.
 *
 * Timing ticks:
 *   - The scheduler runs off constants such as `MIDI_TASK_INTERVAL`,
 *     `LED_TASK_INTERVAL`, and `ENVELOPE_TASK_INTERVAL`, all in milliseconds.
 *     These keep MIDI, blinkenlights, and envelope followers marching in time.
 *
 * EEPROM stash:
 *   - Addresses like `EEPROM_FILTER_FREQ`, `EEPROM_FILTER_Q`, and
 *     `EEPROM_SLOT_BASE` mark where we squirrel settings away so the rig
 *     remembers its vibe after power-down.
 *
 * Pin legends and memory map live in `docs/PinMap.md` and `docs/EEPROMLayout.md`.
 */
#ifndef GLOBALS_H
#define GLOBALS_H

#include <Arduino.h>
#include <utility>
#include <array>
#include <cstddef>

#include "MIDITypes.h"
class ConfigManager;
extern ConfigManager configManager;

class MIDIHandler;
extern MIDIHandler midiHandler;

class Arpeggiator;
extern Arpeggiator arpeggiator;

inline constexpr uint16_t CONFIG_VERSION = 0x0006; //!< EEPROM schema version

extern uint32_t g_resetCause;    //!< Raw reset cause register
extern uint16_t g_brownoutCount; //!< Persistent brownout counter

/**
 * System-wide counters for performance hiccups and watchdog events. Updated by
 * the hot paths in firmware_main.cpp and MIDIHandler so we can surface them on
 * the OLED, WebSerial, or anywhere else that wants to tattle on overloads.
 */
struct SystemDiagnostics {
    volatile uint32_t uartOverrunCount = 0; //!< Hardware UART overruns latched from LPUART6
    volatile uint32_t midiDropCount =
        0; //!< Messages we intentionally dropped (bad data, unsupported types)
    volatile uint32_t midiTaskOverrunCount =
        0; //!< processIncomingMIDI calls that ran longer than the 1 ms budget
    volatile uint32_t loopOverrunCount = 0; //!< Main loop spins that busted the 1 ms soft ceiling
    volatile uint32_t maxLoopMicros =
        0; //!< Worst-case loop duration observed during the last sampling window
    volatile uint32_t lastLoopMicros =
        0; //!< Duration of the most recent loop iteration in microseconds
    volatile uint32_t maxProcessMidiMicros =
        0; //!< Slowest MIDI service pass observed in microseconds
    volatile uint32_t lastProcessMidiMicros = 0; //!< Duration of the most recent MIDI service pass
};

extern SystemDiagnostics g_systemDiagnostics;

/**
 * Bundle every pin and scheduler tick that describes the hardware.
 * Defaults live in Globals.cpp but can be patched at build or run time.
 */
struct HardwareConfig {
    uint8_t ledPin;
    uint8_t statusLedPin;
    uint8_t rowDriverPin;
    uint16_t slotLedCount;
    uint8_t efLedCount;
    uint8_t potLedCount;
    uint8_t numButtons;
    uint8_t midiTaskInterval;
    uint8_t serialTaskInterval;
    uint8_t ledTaskInterval;
    uint8_t envelopeTaskInterval;
    uint8_t muxrPins[4];
    uint8_t muxcPins[4];
    uint8_t buttonMuxAnalogPin;
    uint8_t potMuxAnalogPin;
    uint8_t vrefAdcPin;
};

extern HardwareConfig hwConfig;
void loadHardwareConfig();

// Derived LED indices
inline uint16_t EF_LED_OFFSET() { return hwConfig.slotLedCount; }
inline uint16_t CONTROL_LED_INDEX() { return EF_LED_OFFSET() + hwConfig.efLedCount; }
inline uint16_t POT_LED_OFFSET() { return CONTROL_LED_INDEX() + 1; }
inline uint16_t NUM_LEDS() {
    return hwConfig.slotLedCount + hwConfig.efLedCount + 1 + hwConfig.potLedCount;
}

inline constexpr uint8_t NUM_BUTTONS = 6;   //!< Number of direct control buttons
inline constexpr uint8_t NUM_ENVELOPES = 6; //!< Envelope followers stalking your signal

/**
 * Baseline offsets for each envelope follower.  These numbers get learned
 * during calibration so the followers know where "silence" sits.
 */
struct EnvelopeConfig {
    float baselines[NUM_ENVELOPES];
};

extern EnvelopeConfig envelopeConfig;

// Legacy aliases for modules awaiting full refactors
inline const uint8_t (&primaryMuxPins)[4] = hwConfig.muxrPins;
inline const uint8_t (&secondaryMuxPins)[4] = hwConfig.muxcPins;
inline uint8_t &buttonMuxAnalogPin = hwConfig.buttonMuxAnalogPin;
inline uint8_t &potMuxAnalogPin = hwConfig.potMuxAnalogPin;
inline uint8_t &VREF_ADC_PIN = hwConfig.vrefAdcPin;
inline uint16_t &SLOT_LED_COUNT = hwConfig.slotLedCount;
inline uint8_t &EF_LED_COUNT = hwConfig.efLedCount;
inline uint8_t &POT_LED_COUNT = hwConfig.potLedCount;

inline constexpr uint16_t OLED_WIDTH = 128;          //!< OLED display width in pixels
inline constexpr uint16_t OLED_HEIGHT = 64;          //!< OLED display height in pixels
inline constexpr uint8_t SSD1306_I2C_ADDRESS = 0x3C; //!< I2C address for the OLED
inline constexpr uint16_t SERIAL_BUFFER_SIZE = 128;  //!< bytes in the serial buffer
inline constexpr unsigned long SERIAL_BAUD = 115200; //!< default USB serial rate
inline constexpr uint8_t POT_RANGE_MIN = 10;         //!< Min pot delta before acting
inline constexpr uint8_t ENV_RANGE_MIN = 5;          //!< Min envelope delta threshold

// ADC scaling from raw reading to volts (3.3V reference, 10-bit ADC)
constexpr float VadcScale = 3.3f / 1023.0f;
// Global storage for measured VREF voltage
extern float g_vref;

// EEPROM storage constants
inline constexpr std::size_t SLOT_EEPROM_SIZE = sizeof(MIDISlot); // bytes required per MIDISlot

inline constexpr uint8_t NUM_PROFILES = 4; // Profiles A-D
inline constexpr uint16_t EEPROM_PROFILE_BLOCK_SIZE = 256;
inline constexpr uint16_t EEPROM_PROFILE_SETTINGS_BLOCK_SIZE = 1024; // Extended profile payload
inline constexpr uint16_t EEPROM_START_ADDRESS = 0;
inline constexpr uint16_t EEPROM_MAGIC_ADDRESS =
    EEPROM_START_ADDRESS + 200; //!< Reserve space for config + magic number
inline constexpr uint16_t EEPROM_MAGIC_PRIMARY = 0xABCD; //!< Validates the main config block
inline constexpr uint16_t EEPROM_MAGIC_BACKUP = 0xDCBA;  //!< Signals a sane backup image

inline constexpr uint16_t EEPROM_EF_BASELINES = EEPROM_MAGIC_ADDRESS + 4;
inline constexpr uint16_t EEPROM_EF_BASELINES_SIZE = NUM_ENVELOPES * sizeof(float);
inline constexpr uint8_t EEPROM_BUFFER_SIZE = 22;

inline constexpr uint16_t EEPROM_BACKUP_START =
    EEPROM_EF_BASELINES + EEPROM_EF_BASELINES_SIZE + EEPROM_BUFFER_SIZE;
inline constexpr uint16_t EEPROM_CONFIG_MIRROR_SIZE = EEPROM_BACKUP_START * 2;

inline constexpr uint16_t EEPROM_SLOT_BASE = EEPROM_CONFIG_MIRROR_SIZE;
inline constexpr uint16_t EEPROM_SLOT_REGION_SIZE =
    static_cast<uint16_t>(SLOT_EEPROM_SIZE * NUM_SLOTS);

inline constexpr uint16_t EEPROM_LEGACY_FILTER_FREQ = 1000;    //!< Pre-schema-4 filter freq slot
inline constexpr uint16_t EEPROM_LEGACY_FILTER_Q = 1004;       //!< Pre-schema-4 filter Q slot
inline constexpr uint16_t EEPROM_LEGACY_BROWNOUT_COUNT = 1008; //!< Old brownout counter slot

inline constexpr uint16_t EEPROM_FILTER_FREQ =
    static_cast<uint16_t>(EEPROM_SLOT_BASE + EEPROM_SLOT_REGION_SIZE);
inline constexpr uint16_t EEPROM_FILTER_Q =
    static_cast<uint16_t>(EEPROM_FILTER_FREQ + sizeof(float));
inline constexpr uint16_t EEPROM_BROWNOUT_COUNT =
    static_cast<uint16_t>(EEPROM_FILTER_Q + sizeof(float));
inline constexpr uint16_t EEPROM_CONFIG_TAIL =
    static_cast<uint16_t>(EEPROM_BROWNOUT_COUNT + sizeof(uint16_t));

inline constexpr uint16_t EEPROM_PROFILE_START(uint8_t id) {
    return (id == 0)
               ? EEPROM_START_ADDRESS
               : static_cast<uint16_t>(EEPROM_CONFIG_TAIL + (id - 1) * EEPROM_PROFILE_BLOCK_SIZE);
}

inline constexpr uint16_t EEPROM_PROFILE_SETTINGS_BASE =
    static_cast<uint16_t>(EEPROM_CONFIG_TAIL + NUM_PROFILES * EEPROM_PROFILE_BLOCK_SIZE);

inline constexpr uint16_t EEPROM_PROFILE_SETTINGS_START(uint8_t id) {
    return static_cast<uint16_t>(EEPROM_PROFILE_SETTINGS_BASE +
                                 id * EEPROM_PROFILE_SETTINGS_BLOCK_SIZE);
}

// clock
constexpr unsigned long CLOCK_TIMEOUT_MS = 2000; // 2 seconds without clock => fallback
extern float g_tappedBPM;
extern bool g_clockOutEnabled;
extern bool g_usbMidiOutEnabled;    //!< USB MIDI stays quiet until these three go down
extern unsigned long lastClockTime; // Timestamp of the most recent MIDI clock tick

// LFO modulation buses (range -1..1 unless otherwise noted).
extern float g_lfoEfGainTrim;
extern float g_lfoArpSwing;
extern float g_lfoLedBrightness;
extern std::array<float, 2> g_lfoValues; //!< Normalized LFO outputs (0..1)

/**
 * Flag flipped once the browser opens a WebSerial stream.
 * WebSerial.cpp only transmits telemetry when this stays true.
 */
extern bool webSerialStreaming;

// Note dynamics from the "Freq" and "Q" control pots
extern int8_t velocityShift;      //!< -64..+63 shove applied to outgoing note velocity
extern uint8_t changeProbability; //!< 0-100% chance a moved pot actually slings a new note

// Global Perlin jitter controls shared by arpeggiator + random EF filters.
struct JitterSettings {
    float depth;      //!< 0..1 scale applied to the noise amplitude
    float smoothness; //!< 0..1 control over Perlin step smoothness
};

extern JitterSettings g_jitterSettings;
extern bool g_jitterTuningActive;
extern bool g_arpEditActive;          //!< True while the arp edit combo is held
extern uint8_t g_activeProfile;       //!< Current profile index (0..NUM_PROFILES-1)
extern bool g_profileChangeRequested; //!< Signal to reload profile data in main loop
extern bool g_profileSaveRequested;   //!< Signal to snapshot current settings into profile

// Direct-wired control buttons use separate GPIOs so they don't
// interfere with the mux select lines.
// Wiring: C0->12, C1->13, C2->14, C3->15, C4->24, C5->25

extern int NORMAL_DISPLAY_TIME;
extern int SHORT_DISPLAY_TIME;

// Analog Routing Grid helpers.  ENVELOPE_ANALOG_PINS exposes the raw Teensy
// analog channel behind each follower index so legacy callers can still talk
// pins while the modern UI traffics strictly in indices.
extern const std::array<int, NUM_ENVELOPES> ENVELOPE_ANALOG_PINS;

/** Map an envelope index (0..NUM_ENVELOPES-1) to its Teensy analog pin. */
int envelopeAnalogPin(uint8_t index);

/** Resolve a Teensy analog pin back to its envelope index, or -1 if unknown. */
int envelopeIndexFromAnalogPin(int analogPin);

inline constexpr size_t ARG_PAIR_COUNT = (NUM_ENVELOPES * (NUM_ENVELOPES - 1)) / 2;
extern const std::array<std::pair<uint8_t, uint8_t>, ARG_PAIR_COUNT> ARG_PAIRS;
extern const size_t ARG_PAIRS_LEN;

void saveSlotEfSettings(uint8_t slotIndex, const MIDISlot::EfSettings &settings);

#endif // GLOBALS_H
