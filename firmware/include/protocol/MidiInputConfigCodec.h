#ifndef PROTOCOL_MIDI_INPUT_CONFIG_CODEC_H
#define PROTOCOL_MIDI_INPUT_CONFIG_CODEC_H

#include <ArduinoJson.h>
#include <array>
#include <cstdint>

#include "MidiInputTypes.h"

class MidiInputRouter;

namespace MidiInputConfigCodec {

struct ParseResult {
    bool ok = true;
    bool specified = false;
    uint8_t count = 0;
    const char *errorCode = nullptr;
    const char *errorMessage = nullptr;
    std::array<MidiInputBinding, MIDI_INPUT_MAX_BINDINGS> bindings{};
};

// Missing midiInputBindings means "preserve the active profile's routes";
// an explicitly empty array means "clear them".
ParseResult parse(JsonObject config);
void write(JsonArray output, const MidiInputRouter &router);
void write(JsonArray output, const MidiInputBinding *bindings, uint8_t count);

} // namespace MidiInputConfigCodec

#endif // PROTOCOL_MIDI_INPUT_CONFIG_CODEC_H
