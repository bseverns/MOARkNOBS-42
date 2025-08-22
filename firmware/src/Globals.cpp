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

// Analog Routing Grid pairings. These tuples let the envelope followers
// cross-pollinate analog channels without touching the patch cables.
const std::pair<int, int> ARG_PAIRS[] = {
    // All pairs beginning with A0
    {A0, A1},
    {A0, A2},
    {A0, A3},
    {A0, A6},
    {A0, A7},

    // Then pairs beginning with A1
    {A1, A0},
    {A1, A2},
    {A1, A3},
    {A1, A6},
    {A1, A7},

    // Then pairs beginning with A2
    {A2, A0},
    {A2, A1},
    {A2, A3},
    {A2, A6},
    {A2, A7},

    // Then pairs beginning with A3
    {A3, A0},
    {A3, A1},
    {A3, A2},
    {A3, A6},
    {A3, A7},

    // Finally the one pair from A6
    {A6, A0},
    {A6, A1},
    {A6, A2},
    {A6, A3},
    {A6, A7}
};

const size_t ARG_PAIRS_LEN = sizeof(ARG_PAIRS) / sizeof(ARG_PAIRS[0]);

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

