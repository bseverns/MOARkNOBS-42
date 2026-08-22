#include "protocol/MidiInputConfigCodec.h"

#include <cstdio>
#include <cstring>

#include "MidiInputRouter.h"

namespace MidiInputConfigCodec {
namespace {
ParseResult failure(const char *code, const char *message) {
    ParseResult result{};
    result.ok = false;
    result.specified = true;
    result.errorCode = code;
    result.errorMessage = message;
    return result;
}
} // namespace

ParseResult parse(JsonObject config) {
    ParseResult result{};
    result.specified = config.containsKey("midiInputBindings");
    if (!result.specified) return result;
    if (!config["midiInputBindings"].is<JsonArray>()) {
        return failure("midi_input_type", "midiInputBindings must be an array");
    }

    JsonArray entries = config["midiInputBindings"].as<JsonArray>();
    if (entries.size() > MIDI_INPUT_MAX_BINDINGS) {
        return failure("midi_input_capacity", "too many MIDI input bindings");
    }

    for (JsonVariant entryVariant : entries) {
        if (!entryVariant.is<JsonObject>()) {
            return failure("midi_input_entry", "MIDI input binding must be an object");
        }
        JsonObject entry = entryVariant.as<JsonObject>();
        if (!entry["source"].is<JsonObject>() || !entry["destination"].is<const char *>()) {
            return failure("midi_input_fields", "binding source or destination missing");
        }

        JsonObject source = entry["source"].as<JsonObject>();
        if (std::strcmp(source["type"] | "", "cc7") != 0) {
            return failure("midi_input_source", "only cc7 input is supported");
        }

        MidiInputBinding binding{};
        MidiInputPort port{};
        MidiInputMode mode{};
        MachineParameterTarget target{};
        uint8_t targetIndex = 0;
        if (!parseMidiInputPort(source["port"] | "any", port) ||
            !parseMidiInputMode(entry["mode"] | "absolute", mode) ||
            !parseMachineParameterTarget(entry["destination"].as<const char *>(), target,
                                         targetIndex)) {
            return failure("midi_input_enum", "unknown MIDI input binding value");
        }

        const int channel = source["channel"] | 0;
        const int controller = source["number"] | -1;
        if (channel < 1 || channel > 16 || controller < 0 || controller > 127) {
            return failure("midi_input_range", "MIDI source is outside its valid range");
        }

        binding.port = static_cast<uint8_t>(port);
        binding.channel = static_cast<uint8_t>(channel);
        binding.controller = static_cast<uint8_t>(controller);
        binding.target = static_cast<uint8_t>(target);
        binding.targetIndex = targetIndex;
        binding.mode = static_cast<uint8_t>(mode);

        if (entry["outputRange"].is<JsonArray>()) {
            JsonArray range = entry["outputRange"].as<JsonArray>();
            if (range.size() != 2 || !range[0].is<int>() || !range[1].is<int>()) {
                return failure("midi_input_output_range",
                               "outputRange must contain two integers");
            }
            const int low = range[0].as<int>();
            const int high = range[1].as<int>();
            if (low < 0 || low > 127 || high < low || high > 127) {
                return failure("midi_input_output_range", "outputRange is invalid");
            }
            binding.minValue = static_cast<uint8_t>(low);
            binding.maxValue = static_cast<uint8_t>(high);
        }

        const char *pickup = entry["pickup"] | "soft";
        if (std::strcmp(pickup, "soft") == 0) {
            binding.flags = MIDI_INPUT_FLAG_SOFT_TAKEOVER;
        } else if (std::strcmp(pickup, "jump") == 0) {
            binding.flags = 0;
        } else {
            return failure("midi_input_pickup", "pickup must be soft or jump");
        }
        result.bindings[result.count++] = binding;
    }
    return result;
}

void write(JsonArray output, const MidiInputBinding *bindings, uint8_t count) {
    if (!bindings) return;
    for (uint8_t i = 0; i < count && i < MIDI_INPUT_MAX_BINDINGS; ++i) {
        const MidiInputBinding &binding = bindings[i];
        JsonObject object = output.createNestedObject();
        JsonObject source = object.createNestedObject("source");
        source["port"] = midiInputPortName(static_cast<MidiInputPort>(binding.port));
        source["type"] = "cc7";
        source["channel"] = binding.channel;
        source["number"] = binding.controller;

        const auto target = static_cast<MachineParameterTarget>(binding.target);
        if (target == MachineParameterTarget::SlotValue) {
            char name[20]{};
            std::snprintf(name, sizeof(name), "slot.%u.value",
                          static_cast<unsigned>(binding.targetIndex));
            object["destination"] = name;
        } else {
            object["destination"] = machineParameterTargetName(target);
        }
        object["mode"] = midiInputModeName(static_cast<MidiInputMode>(binding.mode));
        JsonArray range = object.createNestedArray("outputRange");
        range.add(binding.minValue);
        range.add(binding.maxValue);
        object["pickup"] =
            (binding.flags & MIDI_INPUT_FLAG_SOFT_TAKEOVER) ? "soft" : "jump";
    }
}

void write(JsonArray output, const MidiInputRouter &router) {
    MidiInputBinding bindings[MIDI_INPUT_MAX_BINDINGS]{};
    uint8_t count = 0;
    for (size_t i = 0; i < router.bindingCount() && count < MIDI_INPUT_MAX_BINDINGS; ++i) {
        if (router.getBinding(i, bindings[count])) ++count;
    }
    write(output, bindings, count);
}

} // namespace MidiInputConfigCodec
