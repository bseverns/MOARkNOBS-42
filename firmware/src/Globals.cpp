// Globals.cpp is the wiring diagram the compiler can read. It corrals all the
// cross-module shared state—hardware pin maps, runtime knobs, and helper
// lookups—so we have one canonical source of truth. The comments aim to demystify
// global data by explaining why each variable lives at this scope and how you
// can override it safely via JSON or header hooks.

#include "Globals.h"
#include <Arduino.h>
#include <algorithm>

#if __has_include(<ArduinoJson.h>)
#if defined(USB_MIDI_STUB)
#include "usb_midi.h"
#endif
#include <ArduinoJson.h>
#endif

#if !defined(UNIT_TEST) && __has_include(<SD.h>)
#include <SD.h>
inline constexpr bool kHasSD = true;
#else
inline constexpr bool kHasSD = false;
#endif

// Default hardware description
HardwareConfig hwConfig = {
    .ledPin = 6,                // WS2812 data pin (docs/PinMap.md: LED_PIN)
    .statusLedPin = 23,         // status LED indicator (docs/PinMap.md: STATUS_LED_PIN)
    .rowDriverPin = 7,          // button matrix row driver (docs/PinMap.md: PIN_ROW_DRV)
    .slotLedCount = 42,         // total WS2812s in slot ring
    .efLedCount = 6,            // envelope follower LEDs riding the strip
    .potLedCount = 3,           // pot position LEDs
    .numButtons = NUM_BUTTONS,  // direct-control buttons on deck
    .midiTaskInterval = 1,      // ms between MIDI service loops
    .serialTaskInterval = 10,   // ms between serial pokes
    .ledTaskInterval = 50,      // ms between LED refresh bursts
    .envelopeTaskInterval = 5,  // ms cadence for envelope tracking
    .muxrPins = {2, 3, 4, 5},   // CD74HC4067 row select lines (docs/PinMap.md: MUXR0..3)
    .muxcPins = {8, 9, 10, 11}, // CD74HC4067 column select lines (docs/PinMap.md: MUXC0..3)
    .buttonMuxAnalogPin = A4,   // button matrix analog read (docs/PinMap.md: BTN_MUX_OUT)
    .potMuxAnalogPin = A5,      // pot matrix analog read (docs/PinMap.md: POT_MUX_OUT)
    .vrefAdcPin = A8,           // VREF sense tap (docs/PinMap.md: VREF_ADC)
};

// Global runtime variables
float g_vref = 1.65f;                 // midpoint voltage reference
float g_tappedBPM = 120.0f;           // last tapped tempo
bool g_clockOutEnabled = false;       // runtime toggle for MIDI clock out
bool g_followExternalClock = true;    // follow external clock unless performer forces INT
bool g_usbMidiOutEnabled = false;     // gated USB MIDI output
unsigned long lastClockTime = 0;      // ms timestamp of the last MIDI clock tick
uint32_t g_resetCause = 0;            // raw reset cause from SRC_SRSR
uint16_t g_brownoutCount = 0;         // persisted brownout counter
uint8_t midiBeatPosition = 0;         // 0-7 beat slot; increments on MIDI clock ticks
bool g_seedboxInteropEnabled = false; // keep SeedBox interop silent unless explicitly enabled
char serialBuffer[SERIAL_BUFFER_SIZE] = {0};     // serial line buffer shared across modules
uint8_t serialBufferIndex = 0;                   // write cursor for `serialBuffer`
bool webSerialStreaming = false;                 // flipped on when the browser wants JSON telemetry
float g_lfoEfGainTrim = 0.0f;                    // LFO-modulated gain trim for envelope followers
float g_lfoArpSwing = 0.0f;                      // LFO-modulated swing amount for the arpeggiator
float g_lfoVelocityShift = 0.0f;                 // LFO-modulated note velocity offset
float g_lfoNoteChance = 0.0f;                    // LFO-modulated note probability offset
float g_lfoArpGate = 0.0f;                       // LFO-modulated arp gate offset
float g_lfoJitterDepth = 0.0f;                   // LFO-modulated jitter depth offset
float g_lfoJitterSmoothness = 0.0f;              // LFO-modulated jitter smoothness offset
std::array<float, 2> g_lfoValues = {0.0f, 0.0f}; // Normalized LFO outputs

// Note dynamics knobs
int8_t velocityShift = 0;
uint8_t changeProbability = 100;
bool g_noteDynamicsRemoteControlActive = false;
bool g_noteDynamicsShiftLatched = false;
bool g_noteDynamicsProbabilityLatched = false;

// Display timing defaults (overridden by firmware_main.cpp when linked).
int NORMAL_DISPLAY_TIME __attribute__((weak)) = 30000;
int SHORT_DISPLAY_TIME __attribute__((weak)) = 10000;

// Perlin jitter defaults
JitterSettings g_jitterSettings = {
    1.0f,
    0.5f,
};
bool g_jitterTuningActive = false;
bool g_jitterRemoteControlActive = false;
bool g_jitterDepthLatched = false;
bool g_jitterSmoothnessLatched = false;
bool g_arpEditActive = false;                  // Arp edit mode held by combo in ButtonManager
uint8_t g_activeProfile = 0;                   // Active profile index (persisted in EEPROM)
bool g_profileChangeRequested = false;         // Flag for main loop profile reload
bool g_profileSaveRequested = false;           // Flag for main loop profile save
uint8_t g_efIdleFloor = EF_IDLE_FLOOR_DEFAULT; // Clamp disconnected/idle EF readings to zero

// Envelope follower calibration stash
EnvelopeConfig envelopeConfig = {{0}};

SystemDiagnostics g_systemDiagnostics;

namespace {
// Analog Routing Grid pairings.  We walk the six EF analog pins at compile time
// and stash every unique (A,B) combo with A<B.  Change the pin list and the
// pairings auto-update—no static table to forget.  The runtime only cares about
// follower indices, so we store index pairs even though the source list is pins.
constexpr std::array<int, NUM_ENVELOPES> kEnvelopePins = {A0, A1, A2, A3, A6, A7};

constexpr std::array<std::pair<uint8_t, uint8_t>, ARG_PAIR_COUNT> buildArgPairs() {
    std::array<std::pair<uint8_t, uint8_t>, ARG_PAIR_COUNT> pairs{};
    size_t idx = 0;
    for (size_t a = 0; a < kEnvelopePins.size(); ++a) {
        for (size_t b = a + 1; b < kEnvelopePins.size(); ++b) {
            pairs[idx++] = {static_cast<uint8_t>(a), static_cast<uint8_t>(b)};
        }
    }
    return pairs;
}

// Let an SD card override a handful of hardware pins/timings without forcing a
// custom compile. This keeps bring-up tweaks in data when possible.
void loadFromJson(HardwareConfig &cfg) {
#if __has_include(<ArduinoJson.h>)
    if (!kHasSD)
        return;
    if (!SD.begin())
        return;
    File f = SD.open("/hardware_config.json");
    if (!f)
        return;
    // Punk rock move: allocate pretty close to what the config file demands,
    // but pad it so the parser has headroom for metadata.
    const size_t capacity = std::max<size_t>(512, f.size() + 64);
    DynamicJsonDocument doc(capacity);
    DeserializationError err = deserializeJson(doc, f);
    if (err) {
        Serial.printf("hardware_config.json parse failed: %s\n", err.c_str());
        f.close();
        return;
    }
    if (doc.containsKey("LED_PIN"))
        cfg.ledPin = doc["LED_PIN"];
    if (doc.containsKey("STATUS_LED_PIN"))
        cfg.statusLedPin = doc["STATUS_LED_PIN"];
    if (doc.containsKey("PIN_ROW_DRV"))
        cfg.rowDriverPin = doc["PIN_ROW_DRV"];
    if (doc.containsKey("MIDI_TASK_INTERVAL"))
        cfg.midiTaskInterval = doc["MIDI_TASK_INTERVAL"];
    if (doc.containsKey("SERIAL_TASK_INTERVAL"))
        cfg.serialTaskInterval = doc["SERIAL_TASK_INTERVAL"];
    if (doc.containsKey("LED_TASK_INTERVAL"))
        cfg.ledTaskInterval = doc["LED_TASK_INTERVAL"];
    if (doc.containsKey("ENVELOPE_TASK_INTERVAL"))
        cfg.envelopeTaskInterval = doc["ENVELOPE_TASK_INTERVAL"];
    f.close();
#endif
}
} // namespace

const std::array<int, NUM_ENVELOPES> ENVELOPE_ANALOG_PINS = kEnvelopePins;
const std::array<std::pair<uint8_t, uint8_t>, ARG_PAIR_COUNT> ARG_PAIRS = buildArgPairs();
const size_t ARG_PAIRS_LEN = ARG_PAIRS.size();

// Convert a follower index into the corresponding Teensy analog pin so runtime
// and diagnostics code never hard-codes the wiring table separately.
int envelopeAnalogPin(uint8_t index) {
    if (index >= kEnvelopePins.size()) {
        return -1;
    }
    return kEnvelopePins[index];
}

// Reverse-lookup helper used when code starts from a raw analog pin and needs
// the follower slot index expected by config and diagnostics layers.
int envelopeIndexFromAnalogPin(int analogPin) {
    for (size_t i = 0; i < kEnvelopePins.size(); ++i) {
        if (kEnvelopePins[i] == analogPin) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

#if __has_include("hardware_config.h")
#include "hardware_config.h"
#endif

// Apply compile-time and optional SD-card hardware overrides in one place
// before any hardware manager captures the shared config.
void loadHardwareConfig() {
#if __has_include("hardware_config.h")
    applyHardwareConfigOverrides(hwConfig);
#endif
    loadFromJson(hwConfig);
}
