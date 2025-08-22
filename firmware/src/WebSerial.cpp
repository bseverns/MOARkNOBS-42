// Tiny helper for WebSerial telemetry.
// Formats slot and envelope data as JSON and spits it over Serial.

#include "WebSerial.h"
#include "Utility.h"
#include "Log.h"
#include <ArduinoJson.h>

void WebSerial::sendStateSnapshot(const PotentiometerManager& pots,
                                  const std::vector<EnvelopeFollower>& envelopes) {
    StaticJsonDocument<1024> doc;
    JsonArray slots = doc.createNestedArray("slots");
    for (uint8_t i = 0; i < NUM_POTS; ++i) {
        slots.add(Utility::mapToMidiValue(pots.getLastValue(i)));
    }

    JsonArray envs = doc.createNestedArray("envelopes");
    for (const auto& env : envelopes) {
        envs.add(env.getEnvelopeLevel());
    }

    String payload;
    serializeJson(doc, payload);
    LOG_PRINTLN(payload);
}

