#include "protocol/ModMatrixReport.h"

#include <ArduinoJson.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "ConfigManager.h"
#include "EfSettingsUtils.h"
#include "EnvelopeFollower.h"
#include "FirmwareState.h"
#include "Globals.h"
#include "LFO/LFOManager.h"
#include "Protocol.h"
#include "version.h"

const char *midiMessageTypeName(MIDIMessageType type);
const char *efFilterLabel(MIDISlot::EfSettings::FilterType type);
const char *argMethodName(uint8_t method);

namespace ModMatrixReport {
namespace {
DMAMEM StaticJsonDocument<32768> modMatrixDoc;

struct MidiCcWriterBucket {
    uint8_t channel = 0;
    uint8_t cc = 0;
    uint8_t count = 0;
    char writers[192] = {0};
};

DMAMEM std::array<MidiCcWriterBucket, 96> midiCcWriters{};
size_t midiCcWriterCount = 0;

struct SlotWriterBucket {
    uint8_t slot = 0;
    uint8_t count = 0;
    char writers[192] = {0};
};

DMAMEM std::array<SlotWriterBucket, NUM_SLOTS> slotWriters{};
size_t slotWriterCount = 0;

FLASHMEM const char *efDestinationModeName(uint8_t mode) {
    switch (static_cast<EfDestinationMode>(mode)) {
    case EfDestinationMode::AddClamp:
        return "add_clamp";
    case EfDestinationMode::Subtract:
        return "subtract";
    case EfDestinationMode::Replace:
        return "replace";
    case EfDestinationMode::Scale:
        return "scale";
    case EfDestinationMode::Centered:
        return "centered";
    }
    return "add_clamp";
}

FLASHMEM const char *lfoShapeName(LFOShape shape) {
    switch (shape) {
    case LFOShape::Sine:
        return "sine";
    case LFOShape::Triangle:
        return "triangle";
    case LFOShape::Saw:
        return "saw";
    case LFOShape::Square:
        return "square";
    case LFOShape::SampleHold:
        return "sample_hold";
    case LFOShape::RandomSlew:
        return "random_slew";
    }
    return "unknown";
}

FLASHMEM const char *lfoSyncRatioName(LFOSyncRatio ratio) {
    switch (ratio) {
    case LFOSyncRatio::Div1:
        return "1/1";
    case LFOSyncRatio::Div2:
        return "1/2";
    case LFOSyncRatio::Div4:
        return "1/4";
    case LFOSyncRatio::Div8:
        return "1/8";
    case LFOSyncRatio::Div16:
        return "1/16";
    case LFOSyncRatio::Div32:
        return "1/32";
    case LFOSyncRatio::Mul2:
        return "x2";
    case LFOSyncRatio::Mul4:
        return "x4";
    }
    return "unknown";
}

FLASHMEM const char *lfoInternalTargetName(LFOInternalTarget target) {
    switch (target) {
    case LFOInternalTarget::EfGainTrim:
        return "ef_gain_trim";
    case LFOInternalTarget::ArpSwing:
        return "arp_swing";
    case LFOInternalTarget::VelocityShift:
        return "velocity_shift";
    case LFOInternalTarget::NoteChance:
        return "note_chance";
    case LFOInternalTarget::ArpGate:
        return "arp_gate";
    case LFOInternalTarget::JitterDepth:
        return "jitter_depth";
    case LFOInternalTarget::JitterSmoothness:
        return "jitter_smoothness";
    }
    return "unknown";
}

FLASHMEM const char *lfoRouteTypeName(LFOManager::Route::Type type) {
    switch (type) {
    case LFOManager::Route::Type::Internal:
        return "internal";
    case LFOManager::Route::Type::MidiCC7:
        return "midi_cc7";
    case LFOManager::Route::Type::MidiCC14:
        return "midi_cc14";
    case LFOManager::Route::Type::Osc:
        return "osc";
    case LFOManager::Route::Type::SlotValue:
        return "slot_value";
    }
    return "unknown";
}

FLASHMEM void resetMidiCcWriterBuckets() {
    midiCcWriterCount = 0;
    for (auto &bucket : midiCcWriters) {
        bucket = MidiCcWriterBucket{};
    }
    slotWriterCount = 0;
    for (auto &bucket : slotWriters) {
        bucket = SlotWriterBucket{};
    }
}

template <typename Bucket> FLASHMEM void appendWriter(Bucket &bucket, const char *writer) {
    if (!writer || writer[0] == '\0') {
        return;
    }
    const size_t used = std::strlen(bucket.writers);
    if (used > 0 && used + 2 < sizeof(bucket.writers)) {
        std::strncat(bucket.writers, ", ", sizeof(bucket.writers) - used - 1);
    }
    const size_t nextUsed = std::strlen(bucket.writers);
    if (nextUsed + 1 < sizeof(bucket.writers)) {
        std::strncat(bucket.writers, writer, sizeof(bucket.writers) - nextUsed - 1);
    }
}

FLASHMEM void registerMidiCcWriter(uint8_t channel, uint8_t cc, const char *writer) {
    if (channel < 1 || channel > 16 || cc > 127) {
        return;
    }
    for (size_t i = 0; i < midiCcWriterCount; ++i) {
        MidiCcWriterBucket &bucket = midiCcWriters[i];
        if (bucket.channel == channel && bucket.cc == cc) {
            if (bucket.count < 0xFF) {
                ++bucket.count;
            }
            appendWriter(bucket, writer);
            return;
        }
    }
    if (midiCcWriterCount >= midiCcWriters.size()) {
        return;
    }
    MidiCcWriterBucket &bucket = midiCcWriters[midiCcWriterCount++];
    bucket.channel = channel;
    bucket.cc = cc;
    bucket.count = 1;
    appendWriter(bucket, writer);
}

FLASHMEM void registerSlotWriter(uint8_t slot, const char *writer) {
    if (slot >= NUM_SLOTS) {
        return;
    }
    for (size_t i = 0; i < slotWriterCount; ++i) {
        SlotWriterBucket &bucket = slotWriters[i];
        if (bucket.slot == slot) {
            if (bucket.count < 0xFF) {
                ++bucket.count;
            }
            appendWriter(bucket, writer);
            return;
        }
    }
    if (slotWriterCount >= slotWriters.size()) {
        return;
    }
    SlotWriterBucket &bucket = slotWriters[slotWriterCount++];
    bucket.slot = slot;
    bucket.count = 1;
    appendWriter(bucket, writer);
}

FLASHMEM void writeSourceLists(JsonObject sources) {
    JsonArray ef = sources.createNestedArray("ef");
    for (uint8_t i = 0; i < NUM_ENVELOPES; ++i) {
        ef.add(i);
    }
    JsonArray lfo = sources.createNestedArray("lfo");
    for (uint8_t i = 0; i < LFOManager::kMaxLFOs; ++i) {
        lfo.add(i);
    }
    JsonArray pot = sources.createNestedArray("pot");
    for (uint8_t i = 0; i < NUM_SLOTS; ++i) {
        pot.add(i);
    }
}

FLASHMEM void writeRouteRange(JsonObject route, uint8_t minValue = 0, uint8_t maxValue = 127) {
    JsonObject range = route.createNestedObject("range");
    range["min"] = minValue;
    range["max"] = maxValue;
}

FLASHMEM void writeLfoRouteLimits(JsonObject limits) {
    const size_t total = lfoManager.routeCount();
    const size_t reported = std::min(total, static_cast<size_t>(PROFILE_MAX_ROUTES));
    limits["lfo_route_capacity"] = PROFILE_MAX_ROUTES;
    limits["lfo_route_total"] = total;
    limits["lfo_route_reported"] = reported;
    limits["lfo_route_truncated"] = total > reported;
}

FLASHMEM int lfoRouteLastValue7(const LFOManager::Route &route, float normalized) {
    float amount = std::clamp(static_cast<float>(route.amount) / 100.0f, -1.0f, 1.0f);
    float shaped = 0.5f + (normalized - 0.5f) * std::fabs(amount);
    if (amount < 0.0f) {
        shaped = 1.0f - shaped;
    }
    shaped = std::clamp(shaped, 0.0f, 1.0f);
    return std::clamp(
        static_cast<int>(std::lround(static_cast<float>(route.minValue) +
                                     shaped * static_cast<float>(route.maxValue - route.minValue))),
        0, 127);
}

FLASHMEM void writeSlotMidiDestination(JsonObject midi, const MIDISlot &slot) {
    midi["type"] = midiMessageTypeName(slot.type);
    midi["channel"] = slot.midiChannel;
    midi["data1"] = slot.data1;
    if (slot.type == MIDIMessageType::CC) {
        midi["cc"] = slot.data1;
    } else if (slot.type == MIDIMessageType::ModWheel) {
        midi["cc"] = 1;
    } else if (slot.type == MIDIMessageType::NRPN || slot.type == MIDIMessageType::RPN) {
        midi["parameter"] = static_cast<uint16_t>(slot.data1) << 7;
    }
}

FLASHMEM void registerSlotCcWriter(const MIDISlot &slot, const char *writer) {
    if (slot.type == MIDIMessageType::CC) {
        registerMidiCcWriter(slot.midiChannel, slot.data1, writer);
    } else if (slot.type == MIDIMessageType::ModWheel) {
        registerMidiCcWriter(slot.midiChannel, 1, writer);
    }
}

FLASHMEM void writePotRoutes(JsonArray routes) {
    const auto &slots = configManager.getSlots();
    for (uint8_t slotIndex = 0; slotIndex < NUM_SLOTS; ++slotIndex) {
        const MIDISlot &slot = slots[slotIndex];
        if (!slot.active || slot.type == MIDIMessageType::OFF) {
            continue;
        }
        char id[16];
        std::snprintf(id, sizeof(id), "pot%u", static_cast<unsigned>(slotIndex));
        char destination[24];
        std::snprintf(destination, sizeof(destination), "slot%u.value",
                      static_cast<unsigned>(slotIndex));
        JsonObject route = routes.createNestedObject();
        route["id"] = id;
        route["source"] = id;
        route["source_type"] = "pot";
        route["transform"] = "direct 0-127";
        route["destination"] = destination;
        route["mode"] = "replace";
        route["exit"] = "midi";
        route["active"] = true;
        route["persisted"] = true;
        writeRouteRange(route);
        JsonObject midi = route.createNestedObject("midi");
        writeSlotMidiDestination(midi, slot);
        registerSlotWriter(slotIndex, id);
        registerSlotCcWriter(slot, id);
    }
}

FLASHMEM void writeEfRoutes(JsonArray routes) {
    for (const auto &entry : potToEnvelopeMap) {
        const int slotIndex = entry.first;
        const MIDISlot::EfSettings &settings = entry.second;
        const int followerIndex = settings.followerIndex;
        if (slotIndex < 0 || slotIndex >= NUM_SLOTS || followerIndex < 0 ||
            followerIndex >= static_cast<int>(envelopeFollowers.size())) {
            continue;
        }
        const MIDISlot &slot = configManager.getSlot(static_cast<uint8_t>(slotIndex));
        char id[24];
        std::snprintf(id, sizeof(id), "ef%d_slot%d", followerIndex, slotIndex);
        char source[12];
        std::snprintf(source, sizeof(source), "ef%d", followerIndex);
        char destination[24];
        std::snprintf(destination, sizeof(destination), "slot%d.value", slotIndex);
        char transform[80];
        std::snprintf(transform, sizeof(transform), "%s gain %.2f smoothing %.2f",
                      efFilterLabel(settings.filterType), static_cast<double>(settings.gain),
                      static_cast<double>(settings.smoothing));

        JsonObject route = routes.createNestedObject();
        route["id"] = id;
        route["source"] = source;
        route["source_type"] = "ef";
        route["transform"] = transform;
        route["destination"] = destination;
        route["mode"] = efDestinationModeName(settings.destinationMode);
        route["exit"] = "midi_cc";
        const bool active = slot.active && envelopeFollowers[followerIndex].getActiveState();
        route["active"] = active;
        route["persisted"] = true;
        route["amount"] = 1.0f;
        writeRouteRange(route);
        JsonObject midi = route.createNestedObject("midi");
        midi["type"] = "CC";
        midi["channel"] = potentiometerManager.getChannel(static_cast<uint8_t>(slotIndex));
        midi["cc"] = potentiometerManager.getCCNumber(static_cast<uint8_t>(slotIndex));
        if (active) {
            registerSlotWriter(static_cast<uint8_t>(slotIndex), id);
            registerMidiCcWriter(midi["channel"].as<uint8_t>(), midi["cc"].as<uint8_t>(), id);
        }

        SlotARGConfig arg = sanitizeSlotArg(slot.arg);
        if (arg.enabled != 0) {
            JsonObject argRoute = routes.createNestedObject();
            char argId[24];
            std::snprintf(argId, sizeof(argId), "arg_slot%d", slotIndex);
            char argSource[24];
            std::snprintf(argSource, sizeof(argSource), "ef%u+ef%u",
                          static_cast<unsigned>(arg.sourceA), static_cast<unsigned>(arg.sourceB));
            argRoute["id"] = argId;
            argRoute["source"] = argSource;
            argRoute["source_type"] = "arg";
            argRoute["transform"] = argMethodName(static_cast<uint8_t>(arg.method));
            argRoute["destination"] = destination;
            argRoute["mode"] = "pre_add_arg";
            argRoute["exit"] = "internal_to_ef";
            argRoute["active"] = slot.active;
            argRoute["persisted"] = true;
            writeRouteRange(argRoute);
        }
    }
}

FLASHMEM void writeLfoTransform(JsonObject routeObj, const LFO &lfo,
                                const LFOManager::Route &route) {
    char transform[128];
    std::snprintf(transform, sizeof(transform), "%s %s lfo_depth %.2f route_depth %.2f%s%s",
                  lfoShapeName(lfo.getShape()), lfo.isBipolar() ? "bipolar" : "unipolar",
                  static_cast<double>(lfo.getDepth()), static_cast<double>(route.depth),
                  lfo.isSyncEnabled() ? " sync " : "",
                  lfo.isSyncEnabled() ? lfoSyncRatioName(lfo.getSyncRatio()) : "");
    routeObj["transform"] = transform;
}

FLASHMEM const char *modCombineModeName(ModCombineMode mode);
FLASHMEM int slotLfoMatrixValue(const SlotLfoLane &lane, uint8_t lfoIndex);

FLASHMEM void writeLfoRoutes(JsonArray routes) {
    const size_t count = std::min(lfoManager.routeCount(), static_cast<size_t>(PROFILE_MAX_ROUTES));
    for (size_t routeIndex = 0; routeIndex < count; ++routeIndex) {
        LFOManager::Route route{};
        if (!lfoManager.getRoute(routeIndex, route) || route.lfoIndex >= LFOManager::kMaxLFOs) {
            continue;
        }
        const LFO &lfo = lfoManager.lfo(route.lfoIndex);
        char id[24];
        std::snprintf(id, sizeof(id), "lfo%u_route%u", static_cast<unsigned>(route.lfoIndex),
                      static_cast<unsigned>(routeIndex));
        char source[12];
        std::snprintf(source, sizeof(source), "lfo%u", static_cast<unsigned>(route.lfoIndex));

        JsonObject routeObj = routes.createNestedObject();
        routeObj["id"] = id;
        routeObj["source"] = source;
        routeObj["source_type"] = "lfo";
        routeObj["route_type"] = lfoRouteTypeName(route.type);
        writeLfoTransform(routeObj, lfo, route);
        routeObj["depth"] = route.depth;
        routeObj["amount"] = route.amount;
        routeObj["minValue"] = route.minValue;
        routeObj["maxValue"] = route.maxValue;
        routeObj["rateLimitMs"] = 9;
        routeObj["persisted"] = true;
        routeObj["active"] = true;
        routeObj["last_value"] =
            lfoRouteLastValue7(route, lfoManager.normalizedValue(route.lfoIndex));
        writeRouteRange(routeObj, route.minValue, route.maxValue);

        switch (route.type) {
        case LFOManager::Route::Type::Internal: {
            char destination[40];
            std::snprintf(destination, sizeof(destination), "internal.%s",
                          lfoInternalTargetName(route.target));
            routeObj["destination"] = destination;
            routeObj["mode"] = "add_bus";
            routeObj["exit"] = "internal";
            break;
        }
        case LFOManager::Route::Type::MidiCC7:
            routeObj["destination"] = "midi.cc";
            routeObj["mode"] = "replace";
            routeObj["exit"] = "midi_cc";
            routeObj["channel"] = route.channel;
            routeObj["cc"] = route.ccMsb;
            registerMidiCcWriter(route.channel, route.ccMsb, id);
            break;
        case LFOManager::Route::Type::MidiCC14:
            routeObj["destination"] = "midi.cc14";
            routeObj["mode"] = "replace";
            routeObj["exit"] = "midi_cc14";
            routeObj["channel"] = route.channel;
            routeObj["cc_msb"] = route.ccMsb;
            routeObj["cc_lsb"] = route.ccLsb;
            registerMidiCcWriter(route.channel, route.ccMsb, id);
            registerMidiCcWriter(route.channel, route.ccLsb, id);
            break;
        case LFOManager::Route::Type::Osc:
            routeObj["destination"] = "osc.lfo";
            routeObj["mode"] = "mirror";
            routeObj["exit"] = "osc";
            break;
        case LFOManager::Route::Type::SlotValue: {
            const uint8_t slotIndex = constrain(route.slotIndex, 0, NUM_SLOTS - 1);
            const MIDISlot &slot = configManager.getSlot(slotIndex);
            const bool shadowed = route.lfoIndex < slot.lfo.lfo.size() &&
                                  slot.lfo.lfo[route.lfoIndex].enabled();
            char destination[24];
            std::snprintf(destination, sizeof(destination), "slot%u.value",
                          static_cast<unsigned>(slotIndex));
            routeObj["destination"] = destination;
            routeObj["slot"] = slotIndex;
            routeObj["mode"] = shadowed ? "legacy_shadowed" : "legacy_replace";
            routeObj["exit"] = "midi";
            routeObj["active"] = slot.active && !shadowed;
            JsonObject midi = routeObj.createNestedObject("midi");
            writeSlotMidiDestination(midi, slot);
            if (slot.active && !shadowed) {
                registerSlotWriter(slotIndex, id);
                registerSlotCcWriter(slot, id);
            }
            break;
        }
        }
    }


    // Fixed slot lanes are compositional inputs, not independent writers.
    // They appear in the matrix without registering a collision against EF.
    for (uint8_t slotIndex = 0; slotIndex < NUM_SLOTS; ++slotIndex) {
        const MIDISlot &slot = configManager.getSlot(slotIndex);
        const SlotLfoConfig config = sanitizeSlotLfoConfig(slot.lfo);
        for (uint8_t lfoIndex = 0; lfoIndex < config.lfo.size(); ++lfoIndex) {
            const SlotLfoLane &lane = config.lfo[lfoIndex];
            if (!lane.enabled()) continue;
            JsonObject routeObj = routes.createNestedObject();
            char id[24];
            std::snprintf(id, sizeof(id), "lfo%u_slot%u", static_cast<unsigned>(lfoIndex),
                          static_cast<unsigned>(slotIndex));
            char source[12];
            std::snprintf(source, sizeof(source), "lfo%u", static_cast<unsigned>(lfoIndex));
            char destination[24];
            std::snprintf(destination, sizeof(destination), "slot%u.value",
                          static_cast<unsigned>(slotIndex));
            routeObj["id"] = id;
            routeObj["source"] = source;
            routeObj["source_type"] = "lfo";
            routeObj["route_type"] = "slot_lane";
            routeObj["destination"] = destination;
            routeObj["slot"] = slotIndex;
            routeObj["mode"] = modCombineModeName(lane.mode());
            routeObj["amount"] = lane.amount;
            routeObj["exit"] = "slot_resolver";
            routeObj["persisted"] = true;
            routeObj["active"] = slot.active;
            routeObj["last_value"] = slotLfoMatrixValue(lane, lfoIndex);
            writeRouteRange(routeObj);
            JsonObject midi = routeObj.createNestedObject("midi");
            writeSlotMidiDestination(midi, slot);
        }
    }
}

FLASHMEM void writeMidiCcConflicts(JsonArray conflicts) {
    for (size_t i = 0; i < midiCcWriterCount; ++i) {
        const MidiCcWriterBucket &bucket = midiCcWriters[i];
        if (bucket.count < 2) {
            continue;
        }
        JsonObject conflict = conflicts.createNestedObject();
        conflict["target"] = "midi.cc";
        conflict["channel"] = bucket.channel;
        conflict["cc"] = bucket.cc;
        conflict["writers"] = bucket.writers;
        char message[96];
        std::snprintf(message, sizeof(message), "%u live modulators write CC %u on channel %u",
                      static_cast<unsigned>(bucket.count), static_cast<unsigned>(bucket.cc),
                      static_cast<unsigned>(bucket.channel));
        conflict["message"] = message;
    }
}

FLASHMEM void writeSlotValueConflicts(JsonArray conflicts) {
    for (size_t i = 0; i < slotWriterCount; ++i) {
        const SlotWriterBucket &bucket = slotWriters[i];
        if (bucket.count < 2) {
            continue;
        }
        JsonObject conflict = conflicts.createNestedObject();
        conflict["target"] = "slot.value";
        conflict["slot"] = bucket.slot;
        conflict["writers"] = bucket.writers;
        char message[96];
        std::snprintf(message, sizeof(message), "%u live modulators write slot %u value",
                      static_cast<unsigned>(bucket.count), static_cast<unsigned>(bucket.slot));
        conflict["message"] = message;
    }
}


FLASHMEM const char *modCombineModeName(ModCombineMode mode) {
    switch (mode) {
    case ModCombineMode::AddClamp: return "add_clamp";
    case ModCombineMode::Subtract: return "subtract";
    case ModCombineMode::Replace: return "replace";
    case ModCombineMode::Scale: return "scale";
    case ModCombineMode::Centered: return "centered";
    }
    return "centered";
}

FLASHMEM int slotLfoMatrixValue(const SlotLfoLane &lane, uint8_t lfoIndex) {
    const float amount = static_cast<float>(lane.amount) / 100.0f;
    const float normalized = constrain(lfoManager.normalizedValue(lfoIndex), 0.0f, 1.0f);
    const float signedValue = constrain(lfoManager.signedValue(lfoIndex), -1.0f, 1.0f);
    const float bipolar = signedValue * amount;
    const int centered = static_cast<int>(
        std::lround(bipolar * (bipolar < 0.0f ? 64.0f : 63.0f)));
    switch (lane.mode()) {
    case ModCombineMode::AddClamp:
        return static_cast<int>(std::lround(normalized * amount * 127.0f));
    case ModCombineMode::Subtract:
        return -static_cast<int>(std::lround(normalized * amount * 127.0f));
    case ModCombineMode::Replace:
        return 64 + centered;
    case ModCombineMode::Scale:
        return static_cast<int>(std::lround(bipolar * 100.0f));
    case ModCombineMode::Centered:
        return centered;
    }
    return 0;
}
} // namespace

FLASHMEM bool build(String &payload) {
    auto &doc = modMatrixDoc;
    doc.clear();
    resetMidiCcWriterBuckets();

    doc["type"] = "mod_matrix";
    doc["command"] = "GET_MOD_MATRIX";
    doc["contract_version"] = 1;
    doc["fw_version"] = FW_VERSION_STR;

    JsonObject sources = doc.createNestedObject("sources");
    writeSourceLists(sources);
    JsonObject limits = doc.createNestedObject("limits");
    writeLfoRouteLimits(limits);
    JsonObject transport = doc.createNestedObject("transport");
    transport["policy"] = "slot_token_bucket";
    transport["din_bytes_per_ms"] = 3;
    transport["initial_bytes"] = 15;
    transport["capacity_bytes"] = 64;
    transport["note_on_priority"] = true;
    transport["continuous_coalescing"] = "latest_per_slot";

    JsonArray routes = doc.createNestedArray("routes");
    writePotRoutes(routes);
    writeEfRoutes(routes);
    writeLfoRoutes(routes);

    JsonArray conflicts = doc.createNestedArray("conflicts");
    writeMidiCcConflicts(conflicts);
    writeSlotValueConflicts(conflicts);

    if (doc.overflowed()) return false;
    payload = "";
    serializeJson(doc, payload);
    return true;
}
} // namespace ModMatrixReport
