// Tiny helper for WebSerial telemetry.
// Formats slot and envelope data as JSON and spits it over Serial.

#include "WebSerial.h"
#include "Utility.h"
#include "Log.h"
#include <ArduinoJson.h>

void WebSerial::sendStateSnapshot(const PotentiometerManager &pots,
                                  const std::vector<EnvelopeFollower> &envelopes,
                                  const SystemDiagnostics &diagnostics) {
    StaticJsonDocument<1024> doc;
    JsonArray slots = doc.createNestedArray("slots");
    for (uint8_t i = 0; i < NUM_POTS; ++i) {
        slots.add(Utility::mapToMidiValue(pots.getLastValue(i)));
    }

    JsonArray envs = doc.createNestedArray("envelopes");
    for (const auto &env : envelopes) {
        envs.add(env.getEnvelopeLevel());
    }

    JsonObject diag = doc.createNestedObject("diagnostics");
    diag["uart_overruns"] = diagnostics.uartOverrunCount;
    diag["midi_drops"] = diagnostics.midiDropCount;
    diag["loop_overruns"] = diagnostics.loopOverrunCount;
    diag["midi_task_overruns"] = diagnostics.midiTaskOverrunCount;
    diag["loop_max_us"] = diagnostics.maxLoopMicros;
    diag["loop_last_us"] = diagnostics.lastLoopMicros;
    diag["midi_isr_max_us"] = diagnostics.maxProcessMidiMicros;
    diag["midi_isr_last_us"] = diagnostics.lastProcessMidiMicros;

    String payload;
    serializeJson(doc, payload);
    LOG_PRINTLN(payload);
}
