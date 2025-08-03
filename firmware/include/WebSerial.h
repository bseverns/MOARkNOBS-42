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
     * Output format:
     * {"slots":[v0,..,v41],"envelopes":[e0,..]}
     */
    static void sendStateSnapshot(const PotentiometerManager& pots,
                                  const std::vector<EnvelopeFollower>& envelopes);
};

#endif // WEBSERIAL_H
