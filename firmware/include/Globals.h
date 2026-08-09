/*
Global juice for the MOAR kNOBS firmware.

Reading role:
  - If a learner starts at `firmware_main.cpp`, this is the first header to
    read for the machine's physical contract: counts, pins, EEPROM layout,
    timing defaults, and cross-cutting scalar state.
  - `FirmwareState.h` is the companion header for the instantiated runtime
    objects that operate on these facts.

LED gangs:
  - `SLOT_LED_COUNT`, `EF_LED_COUNT`, and `POT_LED_COUNT` define the three
    clans of pixels on the strip.  Offsets like `EF_LED_OFFSET` and
    `POT_LED_OFFSET` stitch them together into one loud `NUM_LEDS` parade.

Timing ticks:
  - The scheduler runs off constants such as `MIDI_TASK_INTERVAL`,
    `LED_TASK_INTERVAL`, and `ENVELOPE_TASK_INTERVAL`, all in milliseconds.
    These keep MIDI, blinkenlights, and envelope followers marching in time.

EEPROM stash:
  - Addresses like `EEPROM_FILTER_FREQ`, `EEPROM_FILTER_Q`, and
    `EEPROM_SLOT_BASE` mark where we squirrel settings away so the rig
    remembers its vibe after power-down.

Pin legends and memory map live in `docs/PinMap.md` and `docs/EEPROMLayout.md`.
*/
#ifndef GLOBALS_H
#define GLOBALS_H

#include <Arduino.h>
#include <utility>
#include <array>
#include <cstddef>

#include "MIDITypes.h"
#include "StorageLayout.h"
class ConfigManager;
extern ConfigManager configManager;

class MIDIHandler;
extern MIDIHandler midiHandler;

class Arpeggiator;
extern Arpeggiator arpeggiator;

inline constexpr uint16_t CONFIG_VERSION = 0x0008; // EEPROM schema version

inline constexpr uint16_t OLED_WIDTH = 128;          // OLED display width in pixels
inline constexpr uint16_t OLED_HEIGHT = 64;          // OLED display height in pixels
inline constexpr uint8_t SSD1306_I2C_ADDRESS = 0x3C; // I2C address for the OLED
inline constexpr uint16_t SERIAL_BUFFER_SIZE = 128;  // bytes in the serial buffer
inline constexpr unsigned long SERIAL_BAUD = 115200; // default USB serial rate
inline constexpr uint8_t POT_RANGE_MIN = 10;         // Min pot delta before acting
inline constexpr uint8_t ENV_RANGE_MIN = 5;          // Min envelope delta threshold
inline constexpr uint8_t EF_IDLE_FLOOR_DEFAULT = 24; // Default EF noise floor clamp (0..127)

extern uint32_t g_resetCause;                 // Raw reset cause register
extern uint16_t g_brownoutCount;              // Persistent brownout counter
extern uint8_t midiBeatPosition;              // Current MIDI beat slot (0..7)
extern bool g_seedboxInteropEnabled;          // Opt-in bridge chatter; off for normal MIDI use
extern char serialBuffer[SERIAL_BUFFER_SIZE]; // Serial input buffer
extern uint8_t serialBufferIndex;             // Cursor into `serialBuffer`

/*
System-wide counters for performance hiccups and watchdog events. Updated by
the hot paths in firmware_main.cpp and MIDIHandler so we can surface them on
the OLED, WebSerial, or anywhere else that wants to tattle on overloads.
*/
struct SystemDiagnostics {
    volatile uint32_t uartOverrunCount = 0; // Hardware UART overruns latched from LPUART6
    volatile uint32_t midiDropCount =
        0; // Messages we intentionally dropped (bad data, unsupported types)
    volatile uint32_t midiTaskOverrunCount =
        0; // processIncomingMIDI calls that ran longer than the 1 ms budget
    volatile uint32_t loopOverrunCount = 0; // Main loop spins that busted the 1 ms soft ceiling
    volatile uint32_t maxLoopMicros =
        0; // Worst-case loop duration observed during the last sampling window
    volatile uint32_t lastLoopMicros =
        0; // Duration of the most recent loop iteration in microseconds
    volatile uint32_t maxProcessMidiMicros =
        0; // Slowest MIDI service pass observed in microseconds
    volatile uint32_t lastProcessMidiMicros = 0; // Duration of the most recent MIDI service pass
    volatile uint32_t midiServiceRequests = 0;   // Timer1 requests raised
    volatile uint32_t midiServiceExecutions = 0; // task-context MIDI service passes
    volatile uint32_t midiServiceCoalesced = 0;  // requests merged into an already-pending pass
    volatile uint8_t midiServiceMaxBacklog = 0;  // high-water mark before a pass
    volatile uint32_t schedulerMissedRuns = 0;   // periodic deadlines skipped by cooperative lanes
    volatile uint32_t schedulerMaxTaskMicros = 0; // longest individual scheduler callback
    volatile uint32_t droppedClockedQuarterEvents = 0; // historical clock beats discarded after a stall
};

extern SystemDiagnostics g_systemDiagnostics;

/*
Bundle the fixed board topology and the scheduler cadence used by the runtime.
The topology is established before setup() by the long-lived hardware managers;
only the task intervals may be tuned during boot.
*/
struct HardwareConfig {
    const uint8_t ledPin;
    const uint8_t statusLedPin;
    const uint8_t rowDriverPin;
    const uint16_t slotLedCount;
    const uint8_t efLedCount;
    const uint8_t potLedCount;
    const uint8_t numButtons;
    uint8_t midiTaskInterval;
    uint8_t serialTaskInterval;
    uint8_t ledTaskInterval;
    uint8_t envelopeTaskInterval;
    const uint8_t muxrPins[4];
    const uint8_t muxcPins[4];
    const uint8_t buttonMuxAnalogPin;
    const uint8_t potMuxAnalogPin;
    const uint8_t vrefAdcPin;
};

struct HardwareRuntimeTuning {
    uint8_t midiTaskInterval;
    uint8_t serialTaskInterval;
    uint8_t ledTaskInterval;
    uint8_t envelopeTaskInterval;
};

inline HardwareRuntimeTuning hardwareRuntimeTuningFrom(const HardwareConfig &cfg) {
    return {cfg.midiTaskInterval, cfg.serialTaskInterval, cfg.ledTaskInterval,
            cfg.envelopeTaskInterval};
}

// Apply only values that are safe to change after global hardware managers have
// been constructed. A zero interval would create an always-due scheduler task,
// so malformed overrides fall back to the minimum supported cadence.
inline void applyHardwareRuntimeTuning(HardwareConfig &cfg, HardwareRuntimeTuning tuning) {
    cfg.midiTaskInterval = tuning.midiTaskInterval == 0 ? 1 : tuning.midiTaskInterval;
    cfg.serialTaskInterval = tuning.serialTaskInterval == 0 ? 1 : tuning.serialTaskInterval;
    cfg.ledTaskInterval = tuning.ledTaskInterval == 0 ? 1 : tuning.ledTaskInterval;
    cfg.envelopeTaskInterval =
        tuning.envelopeTaskInterval == 0 ? 1 : tuning.envelopeTaskInterval;
}

extern HardwareConfig hwConfig;
void loadHardwareRuntimeTuning();

// Derived LED indices
inline uint16_t EF_LED_OFFSET() { return hwConfig.slotLedCount; }
inline uint16_t CONTROL_LED_INDEX() { return EF_LED_OFFSET() + hwConfig.efLedCount; }
inline uint16_t POT_LED_OFFSET() { return CONTROL_LED_INDEX() + 1; }
inline uint16_t NUM_LEDS() {
    return hwConfig.slotLedCount + hwConfig.efLedCount + 1 + hwConfig.potLedCount;
}

inline constexpr uint8_t NUM_POTS = 42;     // Analog pot count driving the ARG
inline constexpr uint8_t NUM_BUTTONS = 6;   // Number of direct control buttons
inline constexpr uint8_t NUM_ENVELOPES = 6; // Envelope followers stalking your signal
static_assert(NUM_ENVELOPES == STORAGE_LAYOUT_NUM_ENVELOPES,
              "Storage layout envelope count drifted from hardware");
static_assert(NUM_ENVELOPES == SLOT_ARG_SOURCE_COUNT,
              "ARG source count drifted from hardware");

/*
Baseline offsets for each envelope follower.  These numbers get learned
during calibration so the followers know where "silence" sits.
*/
struct EnvelopeConfig {
    float baselines[NUM_ENVELOPES];
};

extern EnvelopeConfig envelopeConfig;

// Legacy aliases for modules awaiting full refactors
inline const uint8_t (&primaryMuxPins)[4] = hwConfig.muxrPins;
inline const uint8_t (&secondaryMuxPins)[4] = hwConfig.muxcPins;
inline const uint8_t &buttonMuxAnalogPin = hwConfig.buttonMuxAnalogPin;
inline const uint8_t &potMuxAnalogPin = hwConfig.potMuxAnalogPin;
inline const uint8_t &VREF_ADC_PIN = hwConfig.vrefAdcPin;
inline const uint16_t &SLOT_LED_COUNT = hwConfig.slotLedCount;
inline const uint8_t &EF_LED_COUNT = hwConfig.efLedCount;
inline const uint8_t &POT_LED_COUNT = hwConfig.potLedCount;

// ADC scaling from raw reading to volts (3.3V reference, 10-bit ADC)
constexpr float VadcScale = 3.3f / 1023.0f;
// Global storage for measured VREF voltage
extern float g_vref;

// clock
constexpr unsigned long CLOCK_TIMEOUT_MS = 2000; // 2 seconds without clock => fallback
extern float g_tappedBPM;
extern bool g_clockOutEnabled;
extern bool
    g_followExternalClock; // True: follow external MIDI clock; false: force internal tapped clock
extern bool g_usbMidiOutEnabled;    // USB MIDI output gate, persisted by ConfigManager.
extern unsigned long lastClockTime; // Timestamp of the most recent MIDI clock tick

// LFO modulation buses (range -1..1 unless otherwise noted).
extern float g_lfoEfGainTrim;
extern float g_lfoArpSwing;
extern float g_lfoVelocityShift;
extern float g_lfoNoteChance;
extern float g_lfoArpGate;
extern float g_lfoJitterDepth;
extern float g_lfoJitterSmoothness;
extern std::array<float, 2> g_lfoValues; // Normalized LFO outputs (0..1)

/*
Flag flipped once the browser opens a WebSerial stream.
WebSerial.cpp only transmits telemetry when this stays true.
*/
extern bool webSerialStreaming;

// Note dynamics from the "Freq" and "Q" control pots
extern int8_t velocityShift;      // -64..+63 shove applied to outgoing note velocity
extern uint8_t changeProbability; // 0-100% chance a moved pot actually slings a new note
extern bool g_noteDynamicsRemoteControlActive;
extern bool g_noteDynamicsShiftLatched;
extern bool g_noteDynamicsProbabilityLatched;

// Global Perlin jitter controls shared by arpeggiator + random EF filters.
struct JitterSettings {
    float depth;      // 0..1 scale applied to the noise amplitude
    float smoothness; // 0..1 control over Perlin step smoothness
};

extern JitterSettings g_jitterSettings;
extern bool g_jitterTuningActive;
extern bool g_jitterRemoteControlActive;
extern bool g_jitterDepthLatched;
extern bool g_jitterSmoothnessLatched;
extern bool g_arpEditActive;          // True while the arp edit combo is held
extern uint8_t g_activeProfile;       // Current profile index (0..NUM_PROFILES-1)
extern bool g_profileChangeRequested; // Signal to reload profile data in main loop
extern bool g_profileSaveRequested;   // Signal to snapshot current settings into profile
extern uint8_t g_efIdleFloor;         // Global EF level at/below which input is treated as idle

// Direct-wired control buttons use separate GPIOs so they don't
// interfere with the mux select lines.
// Wiring: C0->12, C1->13, C2->14, C3->15, C4->24, C5->25

extern int NORMAL_DISPLAY_TIME;
extern int SHORT_DISPLAY_TIME;

// Analog Routing Grid helpers.  ENVELOPE_ANALOG_PINS exposes the raw Teensy
// analog channel behind each follower index so legacy callers can still talk
// pins while the modern UI traffics strictly in indices.
extern const std::array<int, NUM_ENVELOPES> ENVELOPE_ANALOG_PINS;

// Map an envelope index (0..NUM_ENVELOPES-1) to its Teensy analog pin.
int envelopeAnalogPin(uint8_t index);

// Resolve a Teensy analog pin back to its envelope index, or -1 if unknown.
int envelopeIndexFromAnalogPin(int analogPin);

inline constexpr size_t ARG_PAIR_COUNT = (NUM_ENVELOPES * (NUM_ENVELOPES - 1)) / 2;
extern const std::array<std::pair<uint8_t, uint8_t>, ARG_PAIR_COUNT> ARG_PAIRS;
extern const size_t ARG_PAIRS_LEN;

void saveSlotEfSettings(uint8_t slotIndex, const MIDISlot::EfSettings &settings);

#endif // GLOBALS_H
