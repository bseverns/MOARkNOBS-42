#ifndef WEBSERIAL_H
#define WEBSERIAL_H

#include <Arduino.h>
#include <vector>
#include "Globals.h"
#include "PotentiometerManager.h"
#include "EnvelopeFollower.h"

class ConfigManager;

class WebSerial {
  public:
    /**
     * Send a JSON snapshot of all slot values and envelope levels.
     * Structure (newline-terminated):
     * {
     *   "slots": [s0, s1, ..., s41],       // 42 values, each 0-127
     *   "envelopes": [e0, e1, ..., e5]     // 6 values, each 0-127
     * }
     * Example payload:
     * {"slots":[0,1,2,...,41],"envelopes":[0,0,0,0,0,0]}
     * Cross-check docs/WebSerial.md for the gritty protocol details.
     */
    static void sendStateSnapshot(const PotentiometerManager &pots,
                                  const std::vector<EnvelopeFollower> &envelopes);

    /**
     * Blast a one-slot JSON patch when firmware rewires a MIDI slot from
     * hardware input (button combo, WebSerial command, whatever). The payload
     * mirrors the slot objects inside GET_CONFIG so the browser can patch its
     * local state without asking for a full dump.
     */
    static void sendSlotPatch(const ConfigManager &config, uint8_t slotIndex);

    /**
     * Tell the WebSerial UI which envelope just latched onto a slot. During
     * those long-press EF assignments we want the browser to redraw the matrix
     * immediately so the human sees what just latched.
     */
    static void sendEnvelopeAssignment(uint8_t slotIndex, int envelopeIndex);
};

#endif // WEBSERIAL_H
