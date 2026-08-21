#include "protocol/ConfigSchema.h"

#include "Globals.h"

FLASHMEM String buildConfigSchema() {
    String s;
    s.reserve(9400);
    s += "{\"$schema\":\"https://json-schema.org/draft/2020-12/schema\",";
    s += "\"schema_version\":";
    s += String(CONFIG_VERSION);
    s += ",\"title\":\"MOARkNOBS Runtime Configuration\",";
    s += "\"type\":\"object\",";
    s += "\"x_mn42\":{\"authority\":\"device\",";
    s += "\"configurator\":\"convenience editor for staged user input\",";
    s += "\"bridge\":\"required when USB/WebSerial transport is unavailable or host integration "
         "needs it\"},";
    s += "\"required\":[\"slots\",\"efSlots\",\"filter\",\"arg\",\"led\"],";
    s += "\"properties\":{";

    s += "\"slots\":{\"type\":\"array\",\"title\":\"Slot Configuration\",";
    s += "\"description\":\"Primary knob-to-MIDI mapping for each slot.\",\"minItems\":";
    s += String(static_cast<int>(NUM_SLOTS));
    s += ",\"maxItems\":";
    s += String(static_cast<int>(NUM_SLOTS));
    s += ",\"items\":{\"type\":\"object\",";
    s += "\"required\":[\"type\",\"midiChannel\",\"data1\",\"efIndex\",\"active\"],";
    s += "\"properties\":{";
    s += "\"type\":{\"type\":\"string\",\"title\":\"Knob -> MIDI message\",";
    s += "\"enum\":[\"OFF\",\"CC\",\"Note\",\"PitchBend\",\"ProgramChange\",\"Aftertouch\",";
    s += "\"ModWheel\",\"NRPN\",\"RPN\",\"SysEx\"]},";
    s += "\"midiChannel\":{\"type\":\"integer\",\"title\":\"MIDI "
         "channel\",\"minimum\":1,\"maximum\":16},";
    s += "\"data1\":{\"type\":\"integer\",\"title\":\"CC/Note "
         "number\",\"minimum\":0,\"maximum\":127},";
    s += "\"arpNote\":{\"type\":\"integer\",\"title\":\"Arp root "
         "note\",\"minimum\":0,\"maximum\":127},";
    s += "\"efIndex\":{\"type\":\"integer\",\"title\":\"Envelope follower "
         "index\",\"minimum\":-1,\"maximum\":";
    s += String(static_cast<int>(NUM_ENVELOPES - 1));
    s += "},";
    s += "\"ef\":{\"type\":\"object\",\"title\":\"Envelope Follower (EF)\",";
    s += "\"required\":[\"index\",\"filter_index\",\"filter_name\",\"frequency\",\"q\",";
    s += "\"oversample\",\"smoothing\",\"baseline\",\"gain\"],\"properties\":{";
    s += "\"index\":{\"type\":\"integer\",\"minimum\":-1,\"maximum\":";
    s += String(static_cast<int>(NUM_ENVELOPES - 1));
    s += "},";
    s += "\"filter_index\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":6},";
    s += "\"filter_name\":{\"type\":\"string\",\"enum\":[\"LINEAR\",\"OPPOSITE_LINEAR\",";
    s += "\"EXPONENTIAL\",\"RANDOM\",\"LOWPASS\",\"HIGHPASS\",\"BANDPASS\"]},";
    s += "\"frequency\":{\"type\":\"number\",\"minimum\":";
    s += String(EF_FILTER_FREQ_MIN_HZ, 1);
    s += ",\"maximum\":";
    s += String(EF_FILTER_FREQ_MAX_HZ, 1);
    s += "},";
    s += "\"q\":{\"type\":\"number\",\"minimum\":";
    s += String(EF_FILTER_Q_MIN, 2);
    s += ",\"maximum\":";
    s += String(EF_FILTER_Q_MAX, 1);
    s += "},";
    s += "\"oversample\":{\"type\":\"integer\",\"minimum\":";
    s += String(static_cast<int>(EF_OVERSAMPLE_MIN));
    s += ",\"maximum\":";
    s += String(static_cast<int>(EF_OVERSAMPLE_MAX));
    s += "},";
    s += "\"smoothing\":{\"type\":\"number\",\"minimum\":0,\"maximum\":1},";
    s += "\"baseline\":{\"type\":\"number\"},\"gain\":{\"type\":\"number\"},";
    s += "\"mode\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":3},";
    s += "\"autoBaseline\":{\"type\":\"boolean\"},";
    s += "\"autoGain\":{\"type\":\"boolean\"},";
    s += "\"attackMs\":{\"type\":\"integer\",\"minimum\":";
    s += String(static_cast<int>(EF_TIME_MIN_MS));
    s += ",\"maximum\":";
    s += String(static_cast<int>(EF_TIME_MAX_MS));
    s += "},";
    s += "\"releaseMs\":{\"type\":\"integer\",\"minimum\":";
    s += String(static_cast<int>(EF_TIME_MIN_MS));
    s += ",\"maximum\":";
    s += String(static_cast<int>(EF_TIME_MAX_MS));
    s += "},";
    s += "\"rmsWindowMs\":{\"type\":\"integer\",\"minimum\":";
    s += String(static_cast<int>(EF_TIME_MIN_MS));
    s += ",\"maximum\":";
    s += String(static_cast<int>(EF_TIME_MAX_MS));
    s += "},";
    s += "\"baselineTauMs\":{\"type\":\"integer\",\"minimum\":";
    s += String(static_cast<int>(EF_TIME_MIN_MS));
    s += ",\"maximum\":";
    s += String(static_cast<int>(EF_TIME_MAX_MS));
    s += "},";
    s += "\"gainTauMs\":{\"type\":\"integer\",\"minimum\":";
    s += String(static_cast<int>(EF_TIME_MIN_MS));
    s += ",\"maximum\":";
    s += String(static_cast<int>(EF_TIME_MAX_MS));
    s += "},";
    s += "\"gateThreshold\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":127},";
    s += "\"gateHysteresis\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":127},";
    s += "\"activityThreshold\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":127},";
    s += "\"gainTarget\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":127},";
    s += "\"destination_mode\":{\"type\":\"string\",\"enum\":[\"add_clamp\",\"subtract\",";
    s += "\"replace\",\"scale\",\"centered\"],\"default\":\"add_clamp\"}},";
    s += "\"additionalProperties\":false},";
    s += "\"active\":{\"type\":\"boolean\",\"title\":\"Enabled\"},";
    s += "\"sysexTemplate\":{\"type\":\"string\",\"title\":\"SysEx template\",\"maxLength\":128},";
    s += "\"arg\":{\"type\":\"object\",\"title\":\"Follower Combiner (ARG)\",";
    s += "\"required\":[\"enabled\",\"method\",\"method_name\",\"sourceA\",\"sourceB\"],";
    s += "\"properties\":{";
    s += "\"enabled\":{\"type\":\"boolean\"},";
    s += "\"method\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":";
    s += String(static_cast<int>(ARGMethod::XORR));
    s += "},";
    s += "\"method_name\":{\"type\":\"string\",\"enum\":[\"PLUS\",\"MIN\",\"PECK\",\"SHAV\",";
    s += "\"SQAR\",\"BABS\",\"TABS\",\"MULT\",\"DIVI\",\"AVG\",\"XABS\",\"MAXX\",";
    s += "\"MINN\",\"XORR\"]},";
    s += "\"sourceA\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":";
    s += String(static_cast<int>(NUM_ENVELOPES - 1));
    s += "},";
    s += "\"sourceB\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":";
    s += String(static_cast<int>(NUM_ENVELOPES - 1));
    s += "}},\"additionalProperties\":false},";
    s += "\"lfo\":{\"type\":\"array\",\"title\":\"Per-slot LFO lanes\",\"minItems\":2,";
    s += "\"maxItems\":2,\"items\":{\"type\":\"object\",\"required\":[\"enabled\",\"mode\",\"amount\"],";
    s += "\"properties\":{\"enabled\":{\"type\":\"boolean\"},";
    s += "\"mode\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":4},";
    s += "\"amount\":{\"type\":\"integer\",\"minimum\":-100,\"maximum\":100}},";
    s += "\"additionalProperties\":false}}";
    s += "},\"additionalProperties\":false}},";

    s += "\"efSlots\":{\"type\":\"array\",\"title\":\"Envelope Assignments\",\"minItems\":";
    s += String(static_cast<int>(NUM_ENVELOPES));
    s += ",\"maxItems\":";
    s += String(static_cast<int>(NUM_ENVELOPES));
    s += ",\"items\":{\"type\":\"object\",\"properties\":{";
    s += "\"slot\":{\"type\":\"integer\",\"minimum\":-1,\"maximum\":";
    s += String(static_cast<int>(NUM_SLOTS - 1));
    s += "},";
    s += "\"slots\":{\"type\":\"array\",\"items\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":";
    s += String(static_cast<int>(NUM_SLOTS - 1));
    s += "},\"uniqueItems\":true}},";
    s += "\"anyOf\":[{\"required\":[\"slot\"]},{\"required\":[\"slots\"]}],";
    s += "\"additionalProperties\":false}},";

    s += "\"filter\":{\"type\":\"object\",\"title\":\"Follower "
         "Filter\",\"required\":[\"type\",\"freq\",\"q\"],";
    s += "\"properties\":{\"type\":{\"type\":\"string\",\"enum\":[\"LINEAR\",\"OPPOSITE_LINEAR\",";
    s += "\"EXPONENTIAL\",\"RANDOM\",\"LOWPASS\",\"HIGHPASS\",\"BANDPASS\"]},";
    s += "\"freq\":{\"type\":\"number\",\"minimum\":";
    s += String(EF_FILTER_FREQ_MIN_HZ, 1);
    s += ",\"maximum\":";
    s += String(EF_FILTER_FREQ_MAX_HZ, 1);
    s += "},";
    s += "\"q\":{\"type\":\"number\",\"minimum\":";
    s += String(EF_FILTER_Q_MIN, 2);
    s += ",\"maximum\":";
    s += String(EF_FILTER_Q_MAX, 1);
    s += "},";
    s += "\"idle_floor\":{\"type\":\"integer\",\"title\":\"EF idle floor\",";
    s += "\"description\":\"Envelope levels at or below this MIDI value are clamped to zero.\",";
    s += "\"minimum\":0,\"maximum\":127,\"default\":";
    s += String(static_cast<int>(EF_IDLE_FLOOR_DEFAULT));
    s += "}},\"additionalProperties\":false},";

    s += "\"arg\":{\"type\":\"object\",\"title\":\"Follower Combiner (ARG)\",";
    s += "\"required\":[\"method\",\"a\",\"b\"],\"properties\":{";
    s += "\"method\":{\"type\":\"string\",\"enum\":[\"PLUS\",\"MIN\",\"PECK\",\"SHAV\",\"SQAR\",";
    s += "\"BABS\",\"TABS\",\"MULT\",\"DIVI\",\"AVG\",\"XABS\",\"MAXX\",\"MINN\",\"XORR\"]},";
    s += "\"a\":{\"type\":\"number\",\"minimum\":0,\"maximum\":";
    s += String(static_cast<int>(NUM_ENVELOPES - 1));
    s += "},";
    s += "\"b\":{\"type\":\"number\",\"minimum\":0,\"maximum\":";
    s += String(static_cast<int>(NUM_ENVELOPES - 1));
    s += "},";
    s += "\"enable\":{\"type\":\"boolean\"}},\"additionalProperties\":false},";

    s += "\"led\":{\"type\":\"object\",\"title\":\"LED "
         "Colors\",\"required\":[\"brightness\",\"color\"],";
    s += "\"properties\":{\"brightness\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":255},";
    s += "\"color\":{\"type\":\"string\",\"pattern\":\"^#([0-9a-fA-F]{6})$\"},";
    s += "\"mode\":{\"type\":\"string\",\"enum\":[\"STATIC\",\"PEAK_HOLD\",\"TRAIL\",";
    s += "\"CLOCK_PULSE\"],\"default\":\"STATIC\"}},\"additionalProperties\":false},";
    s += "\"envelopeMode\":{\"type\":\"string\",\"enum\":[\"LINEAR\",\"EXPONENTIAL\",\"LOG\"],";
    s += "\"default\":\"LINEAR\"}";
    s += "},\"additionalProperties\":false}";
    return s;
}
