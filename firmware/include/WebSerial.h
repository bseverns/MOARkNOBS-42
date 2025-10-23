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
     *   "envelopes": [e0, e1, ..., e5],    // 6 values, each 0-127
     *   "diagnostics": {
     *     "uart_overruns": <int>,         // DIN UART overruns caught since boot
     *     "midi_drops": <int>,            // Messages we refused (bad framing, etc)
     *     "loop_overruns": <int>,         // Main loop spins that broke the 1 ms budget
     *     "midi_task_overruns": <int>,    // processIncomingMIDI calls running >1 ms
     *     "loop_max_us": <int>,           // Worst loop duration in the last window
     *     "loop_last_us": <int>,          // Duration of the most recent loop spin
     *     "midi_isr_max_us": <int>,       // Slowest MIDI service pass in microseconds
     *     "midi_isr_last_us": <int>       // Latest MIDI service duration in microseconds
     *   }
     * }
     * Example payload:
     * {"slots":[0,1,2,...,41],"envelopes":[0,0,0,0,0,0],"diagnostics":{"loop_max_us":702}}
     * Cross-check docs/WebSerial.md for the gritty protocol details.
     */
    static void sendStateSnapshot(const PotentiometerManager &pots,
                                  const std::vector<EnvelopeFollower> &envelopes,
                                  const SystemDiagnostics &diagnostics);
};

#endif // WEBSERIAL_H
