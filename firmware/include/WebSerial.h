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

extern bool webSerialStreaming;

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
     * Emit a one-slot config patch when firmware rewires a MIDI slot from
     * hardware input (button combo, WebSerial command, whatever). The payload
     * mirrors GET_CONFIG schema fields so the browser can merge the change
     * without pulling a full dump.
     */
    static void sendSlotPatch(const ConfigManager &config, uint8_t slotIndex);

    /**
     * Emit an envelope assignment patch for a single slot so the WebSerial UI
     * can redraw the routing matrix immediately.
     */
    static void sendEnvelopeAssignment(uint8_t slotIndex, int envelopeIndex);

    /**
     * Emit a filter configuration patch (type/frequency/Q).
     */
    static void sendFilterPatch(EnvelopeFollower::FilterType type, float freq, float q);

    /**
     * Emit an ARG configuration patch.
     */
    static void sendArgPatch(uint8_t method, bool enable, uint8_t envA, uint8_t envB);

    /**
     * Emit diagnostic counters whenever the firmware spots a hiccup.
     * Optionally slap on a reason label ("midi_overflow", "loop_overrun", etc.).
     * Pass force=true when answering CLI requests so the payload prints even
     * before WebSerial streaming has been armed by HELLO.
     */
    static void sendDiagnostics(const DiagnosticStats &stats, const char *reason = nullptr,
                                bool force = false);
};

#endif // WEBSERIAL_H
