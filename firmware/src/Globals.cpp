#include "Globals.h"
#include <Arduino.h>

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
    .ledPin = 6,
    .statusLedPin = 23,
    .rowDriverPin = 7,
    .slotLedCount = 42,
    .efLedCount = 6,
    .potLedCount = 3,
    .numButtons = NUM_BUTTONS,
    .midiTaskInterval = 1,
    .serialTaskInterval = 10,
    .ledTaskInterval = 50,
    .envelopeTaskInterval = 5,
    .muxrPins = {2, 3, 4, 5},
    .muxcPins = {8, 9, 10, 11},
    .buttonMuxAnalogPin = A4,
    .potMuxAnalogPin = A5,
    .vrefAdcPin = A8,
};

// Global runtime variables
float g_vref       = 1.65f;    // midpoint voltage reference
float g_tappedBPM  = 120.0f;   // last tapped tempo
bool  g_clockOutEnabled = false; // runtime toggle for MIDI clock out
bool  g_usbMidiOutEnabled = false; // gated USB MIDI output
unsigned long lastClockTime = 0;  // ms timestamp of the last MIDI clock tick
uint32_t g_resetCause = 0;        // raw reset cause from SRC_SRSR
uint16_t g_brownoutCount = 0;     // persisted brownout counter

// Note dynamics knobs
int8_t  velocityShift   = 0;
uint8_t changeProbability = 100;

// Envelope follower calibration stash
EnvelopeConfig envelopeConfig = { {0} };

// Analog Routing Grid pairings.  We walk the six EF analog pins at compile time
// and stash every unique (A,B) combo with A<B.  Change the pin list and the
// pairings auto-update—no static table to forget.
namespace {
constexpr std::array<int, NUM_ENVELOPES> kArgPins = {A0, A1, A2, A3, A6, A7};

constexpr std::array<std::pair<int, int>, ARG_PAIR_COUNT> buildArgPairs() {
    std::array<std::pair<int, int>, ARG_PAIR_COUNT> pairs{};
    size_t idx = 0;
    for (size_t a = 0; a < kArgPins.size(); ++a) {
        for (size_t b = a + 1; b < kArgPins.size(); ++b) {
            pairs[idx++] = {kArgPins[a], kArgPins[b]};
        }
    }
    return pairs;
}
} // namespace

const std::array<std::pair<int, int>, ARG_PAIR_COUNT> ARG_PAIRS = buildArgPairs();
const size_t ARG_PAIRS_LEN = ARG_PAIRS.size();

static void loadFromJson(HardwareConfig& cfg) {
#if __has_include(<ArduinoJson.h>)
    if (!kHasSD) return;
    if (!SD.begin()) return;
    File f = SD.open("/hardware_config.json");
    if (!f) return;
    // Punk rock move: allocate exactly what the config file demands.
    // The old 256B static doc would thrash if the JSON grew; this way we
    // size the DynamicJsonDocument based on the file's byte count and keep
    // the stack chill.
    DynamicJsonDocument doc(f.size());
    DeserializationError err = deserializeJson(doc, f);
    if (err) { f.close(); return; }
    if (doc.containsKey("LED_PIN")) cfg.ledPin = doc["LED_PIN"];
    if (doc.containsKey("STATUS_LED_PIN")) cfg.statusLedPin = doc["STATUS_LED_PIN"];
    if (doc.containsKey("PIN_ROW_DRV")) cfg.rowDriverPin = doc["PIN_ROW_DRV"];
    if (doc.containsKey("MIDI_TASK_INTERVAL")) cfg.midiTaskInterval = doc["MIDI_TASK_INTERVAL"];
    if (doc.containsKey("SERIAL_TASK_INTERVAL")) cfg.serialTaskInterval = doc["SERIAL_TASK_INTERVAL"];
    if (doc.containsKey("LED_TASK_INTERVAL")) cfg.ledTaskInterval = doc["LED_TASK_INTERVAL"];
    if (doc.containsKey("ENVELOPE_TASK_INTERVAL")) cfg.envelopeTaskInterval = doc["ENVELOPE_TASK_INTERVAL"];
    f.close();
#endif
}

#if __has_include("hardware_config.h")
#include "hardware_config.h"
#endif

void loadHardwareConfig() {
#if __has_include("hardware_config.h")
    applyHardwareConfigOverrides(hwConfig);
#endif
    loadFromJson(hwConfig);
}

