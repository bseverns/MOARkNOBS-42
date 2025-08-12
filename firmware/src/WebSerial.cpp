// Tiny helper for WebSerial telemetry.
// Formats slot and envelope data as JSON and spits it over Serial.

#include "WebSerial.h"
#include "Utility.h"
#include "Log.h"

void WebSerial::sendStateSnapshot(const PotentiometerManager& pots,
                                  const std::vector<EnvelopeFollower>& envelopes) {
    LOG_PRINT("{\"slots\":[");
    for (uint8_t i = 0; i < NUM_POTS; ++i) {
        LOG_PRINT(Utility::mapToMidiValue(pots.getLastValue(i)));
           if (i < NUM_POTS - 1) {
            LOG_PRINT(',');
        }
    }
    LOG_PRINT("],\"envelopes\":[");
    for (size_t i = 0; i < envelopes.size(); ++i) {
        LOG_PRINT(envelopes[i].getEnvelopeLevel());
         if (i < envelopes.size() - 1) {
            LOG_PRINT(',');
        }
    }
    LOG_PRINTLN("]}");
}

