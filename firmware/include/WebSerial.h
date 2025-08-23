#ifndef WEBSERIAL_H
#define WEBSERIAL_H

#include <Arduino.h>
#include <vector>
#include "Globals.h"
#include "PotentiometerManager.h"
#include "EnvelopeFollower.h"

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
};

#endif // WEBSERIAL_H
