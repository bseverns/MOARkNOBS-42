#include "Globals.h"

#if __has_include(<ArduinoJson.h>)
#include <ArduinoJson.h>
#include <SD.h>
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

static void loadFromJson(HardwareConfig& cfg) {
#if __has_include(<ArduinoJson.h>)
    if (!SD.begin()) return;
    File f = SD.open("/hardware_config.json");
    if (!f) return;
    StaticJsonDocument<256> doc;
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

