#ifndef WEBSERIAL_H
#define WEBSERIAL_H

#include <Arduino.h>
#include <vector>
#include "Globals.h"
#include "PotentiometerManager.h"
#include "EnvelopeFollower.h"
#include "MIDITypes.h"

class ConfigManager;

class ConfigManager;

class WebSerial {
  public:
    /**
     * Send a JSON snapshot of all slot values and envelope levels.
     * Structure (newline-terminated):
     * {
     *   "slots": [s0, s1, ..., s41],       // 42 values, each 0-127
     *   "envelopes": [e0, e1, ..., e5],    // 6 values, each 0-127
     *   "currentSlot": <int>,              // -1 if nothing is armed, otherwise 0-41
     *   "argMethod": "<label>",          // e.g. "PLUS" or "MULT"
     *   "efStatus": [0|1, ...],           // envelope follower enable flags
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
     * {"slots":[0,1,2,...,41],"envelopes":[0,0,0,0,0,0],"currentSlot":5,
     *  "argMethod":"PLUS","efStatus":[1,0,0,0,0,1],
     *  "diagnostics":{"loop_max_us":702}}
     * Cross-check docs/WebSerial.md for the gritty protocol details.
     */
    static void sendStateSnapshot(const PotentiometerManager &pots,
                                  const std::vector<EnvelopeFollower> &envelopes,
                                  const ConfigManager &config, uint8_t currentSlot,
                                  const SystemDiagnostics &diagnostics);

    /**
     * Emit a compact config patch describing the current state of a slot.
     * Mirrors the schema used by the WebSerial editor so diffs line up.
     */
    static void sendSlotPatch(const ConfigManager &config, uint8_t slotIndex);

    /**
     * Emit an envelope assignment patch for a single slot.
     * Lets the browser repaint routing badges without reloading everything.
     */
    static void sendEnvelopeAssignment(uint8_t slotIndex, int envelopeIndex);

    /**
     * Emit a filter configuration patch (type/frequency/Q) for the active follower.
     * Keeps the UI in lockstep when filters are tuned from the hardware side.
     */
    static void sendFilterPatch(EnvelopeFollower::FilterType type, float freq, float q);

    /**
     * Emit an ARG configuration patch.
     * Broadcasts method, enable flag, and the paired envelopes so remote editors sync up.
     */
    static void sendArgPatch(uint8_t method, bool enable, uint8_t envA, uint8_t envB);
};

#endif // WEBSERIAL_H
