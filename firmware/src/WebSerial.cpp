// Tiny helper for WebSerial telemetry.
// Formats slot and envelope data as JSON and spits it over Serial.

#include "WebSerial.h"
#include "Utility.h"

void WebSerial::sendStateSnapshot(const PotentiometerManager& pots,
                                  const std::vector<EnvelopeFollower>& envelopes) {
    Serial.print("{\"slots\":[");
    for (uint8_t i = 0; i < NUM_POTS; ++i) {
        Serial.print(Utility::mapToMidiValue(pots.getLastValue(i)));
        if (i < NUM_POTS - 1) Serial.print(',');
    }
    Serial.print("],\"envelopes\":[");
    for (size_t i = 0; i < envelopes.size(); ++i) {
        Serial.print(envelopes[i].getEnvelopeLevel());
        if (i < envelopes.size() - 1) Serial.print(',');
    }
    Serial.println("]}");
}

