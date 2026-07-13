#include "protocol/ProtocolSimpleHandlers.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstring>

#include "BoardPowerProfile.h"
#include "Arpeggiator.h"
#include "ConfigManager.h"
#include "DiagnosticRecord.h"
#include "EfSettingsUtils.h"
#include "EnvelopeFollower.h"
#include "FirmwareState.h"
#include "Globals.h"
#include "LFO/LFOManager.h"
#include "Log.h"
#include "MIDIHandler.h"
#include "Modes.h"
#include "Protocol.h"
#include "protocol/ManifestReport.h"
#include "protocol/SysExTemplateCodec.h"
#include "version.h"

// ProtocolSimpleHandlers.cpp is the direct GET/SET lane for host requests that
// do not need the heavier profile, scene, or bulk-config submachines.
//
// Reading order:
// 1. deprecated compatibility shims
// 2. identity/config export reads
// 3. live runtime inspection reads
// 4. direct live-control writes

const char *midiMessageTypeName(MIDIMessageType type);
const char *envelopeFilterName(EnvelopeFollower::FilterType type);
const char *efFilterLabel(MIDISlot::EfSettings::FilterType type);
const char *argMethodName(uint8_t method);
const char *envelopeModeName(uint8_t mode);

namespace ProtocolSimpleHandlers {
namespace {
// GET_CONFIG materializes the full live config tree, which is much larger than
// the hot state we want to keep in RAM1. Park the scratch document in RAM2.
DMAMEM StaticJsonDocument<65536> getConfigDoc;
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

const char *efDestinationModeName(uint8_t mode) {
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

const char *arpShapeName(Arpeggiator::Shape shape) {
    switch (shape) {
    case Arpeggiator::UP:
        return "up";
    case Arpeggiator::DOWN:
        return "down";
    case Arpeggiator::UPDOWN:
        return "up_down";
    case Arpeggiator::RANDOM:
        return "random";
    case Arpeggiator::DRUNK:
        return "drunk";
    case Arpeggiator::EUCLIDEAN:
        return "euclidean";
    }
    return "up";
}

const char *lfoShapeName(LFOShape shape) {
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

const char *lfoSyncRatioName(LFOSyncRatio ratio) {
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

const char *lfoInternalTargetName(LFOInternalTarget target) {
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

const char *lfoRouteTypeName(LFOManager::Route::Type type) {
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

void resetMidiCcWriterBuckets() {
    midiCcWriterCount = 0;
    for (auto &bucket : midiCcWriters) {
        bucket = MidiCcWriterBucket{};
    }
    slotWriterCount = 0;
    for (auto &bucket : slotWriters) {
        bucket = SlotWriterBucket{};
    }
}

template <typename Bucket> void appendWriter(Bucket &bucket, const char *writer) {
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

void registerMidiCcWriter(uint8_t channel, uint8_t cc, const char *writer) {
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

void registerSlotWriter(uint8_t slot, const char *writer) {
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

void writeSourceLists(JsonObject sources) {
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

void writeRouteRange(JsonObject route, uint8_t minValue = 0, uint8_t maxValue = 127) {
    JsonObject range = route.createNestedObject("range");
    range["min"] = minValue;
    range["max"] = maxValue;
}

void writeLfoRouteLimits(JsonObject limits) {
    const size_t total = lfoManager.routeCount();
    const size_t reported = std::min(total, static_cast<size_t>(PROFILE_MAX_ROUTES));
    limits["lfo_route_capacity"] = PROFILE_MAX_ROUTES;
    limits["lfo_route_total"] = total;
    limits["lfo_route_reported"] = reported;
    limits["lfo_route_truncated"] = total > reported;
}

int lfoRouteLastValue7(const LFOManager::Route &route, float normalized) {
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

void writeSlotMidiDestination(JsonObject midi, const MIDISlot &slot) {
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

void registerSlotCcWriter(const MIDISlot &slot, const char *writer) {
    if (slot.type == MIDIMessageType::CC) {
        registerMidiCcWriter(slot.midiChannel, slot.data1, writer);
    } else if (slot.type == MIDIMessageType::ModWheel) {
        registerMidiCcWriter(slot.midiChannel, 1, writer);
    }
}

void writePotRoutes(JsonArray routes) {
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

void writeEfRoutes(JsonArray routes) {
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

void writeLfoTransform(JsonObject routeObj, const LFO &lfo, const LFOManager::Route &route) {
    char transform[128];
    std::snprintf(transform, sizeof(transform), "%s %s lfo_depth %.2f route_depth %.2f%s%s",
                  lfoShapeName(lfo.getShape()), lfo.isBipolar() ? "bipolar" : "unipolar",
                  static_cast<double>(lfo.getDepth()), static_cast<double>(route.depth),
                  lfo.isSyncEnabled() ? " sync " : "",
                  lfo.isSyncEnabled() ? lfoSyncRatioName(lfo.getSyncRatio()) : "");
    routeObj["transform"] = transform;
}

void writeLfoRoutes(JsonArray routes) {
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
            char destination[24];
            std::snprintf(destination, sizeof(destination), "slot%u.value",
                          static_cast<unsigned>(slotIndex));
            routeObj["destination"] = destination;
            routeObj["slot"] = slotIndex;
            routeObj["mode"] = "replace";
            routeObj["exit"] = "midi";
            routeObj["active"] = slot.active;
            JsonObject midi = routeObj.createNestedObject("midi");
            writeSlotMidiDestination(midi, slot);
            if (slot.active) {
                registerSlotWriter(slotIndex, id);
                registerSlotCcWriter(slot, id);
            }
            break;
        }
        }
    }
}

void writeMidiCcConflicts(JsonArray conflicts) {
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

void writeSlotValueConflicts(JsonArray conflicts) {
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

// Pot mappings are the simplest "physical controls -> MIDI lane" truth a host can inspect.
void writePotMappings(JsonArray pots) {
    for (uint8_t i = 0; i < configManager.getNumPots(); ++i) {
        JsonObject pot = pots.createNestedObject();
        pot["index"] = i;
        pot["channel"] = configManager.getPotChannel(i);
        pot["cc"] = configManager.getPotCCNumber(i);
    }
}

// Each slot export answers "what message does this control send, and what modulation owns it?"
void writeSlotEfConfig(JsonObject ef, const MIDISlot &slot) {
    ef["index"] = slot.ef.followerIndex;
    ef["filter_index"] = static_cast<uint8_t>(slot.efSettings.filterType);
    ef["filter_name"] = efFilterLabel(slot.efSettings.filterType);
    ef["frequency"] = slot.efSettings.frequency;
    ef["q"] = slot.efSettings.q;
    ef["oversample"] = slot.efSettings.oversample;
    ef["smoothing"] = slot.efSettings.smoothing;
    ef["baseline"] = slot.efSettings.baseline;
    ef["gain"] = slot.efSettings.gain;
    ef["mode"] = slot.efSettings.efMode;
    ef["destination_mode"] = slot.efSettings.destinationMode;
    ef["destination_mode_name"] = efDestinationModeName(slot.efSettings.destinationMode);
    ef["auto_baseline"] = slot.efSettings.autoBaseline != 0;
    ef["auto_gain"] = slot.efSettings.autoGain != 0;
    ef["attack_ms"] = slot.efSettings.attackMs;
    ef["release_ms"] = slot.efSettings.releaseMs;
    ef["rms_ms"] = slot.efSettings.rmsWindowMs;
    ef["baseline_tau_ms"] = slot.efSettings.baselineTauMs;
    ef["gain_tau_ms"] = slot.efSettings.gainTauMs;
    ef["gate_threshold"] = slot.efSettings.gateThreshold;
    ef["gate_hysteresis"] = slot.efSettings.gateHysteresis;
    ef["activity_threshold"] = slot.efSettings.activityThreshold;
    ef["gain_target"] = slot.efSettings.gainTarget;
}

void writeSlotEnvelopePayload(JsonObject efPayload, uint8_t slotIndex) {
    SlotEnvelopePayload payload = configManager.getSlotEnvelopePayload(slotIndex);
    efPayload["type"] = payload.filterType;
    efPayload["type_name"] =
        envelopeFilterName(static_cast<EnvelopeFollower::FilterType>(payload.filterType));
    efPayload["freq"] = payload.frequency;
    efPayload["q"] = payload.q;
}

void writeSlotArgConfig(JsonObject argObj, const MIDISlot &slot) {
    SlotARGConfig arg = sanitizeSlotArg(slot.arg);
    argObj["enabled"] = arg.enabled != 0;
    argObj["method"] = static_cast<uint8_t>(arg.method);
    argObj["method_name"] = argMethodName(static_cast<uint8_t>(arg.method));
    argObj["sourceA"] = arg.sourceA;
    argObj["sourceB"] = arg.sourceB;
}

void writeSlotConfig(JsonArray slots, uint8_t slotIndex, const MIDISlot &slot) {
    JsonObject slotObj = slots.createNestedObject();
    slotObj["index"] = slotIndex;
    slotObj["type"] = static_cast<uint8_t>(slot.type);
    slotObj["type_name"] = midiMessageTypeName(slot.type);
    slotObj["channel"] = slot.midiChannel;
    slotObj["data1"] = slot.data1;
    slotObj["ef_index"] = slot.ef.followerIndex;
    JsonObject ef = slotObj.createNestedObject("ef");
    writeSlotEfConfig(ef, slot);
    slotObj["active"] = slot.active;
    slotObj["arp_note"] = slot.arpNote;
    slotObj["arpNote"] = slot.arpNote;
    slotObj["sysexTemplate"] = formatSysExTemplate(slot);
    JsonObject efPayload = slotObj.createNestedObject("ef_payload");
    writeSlotEnvelopePayload(efPayload, slotIndex);
    JsonObject argObj = slotObj.createNestedObject("arg");
    writeSlotArgConfig(argObj, slot);
}

// EF slot mappings tell the host which knobs are currently feeding each follower voice.
void writeEfSlotMappings(JsonArray efSlots) {
    for (uint8_t followerIndex = 0; followerIndex < NUM_ENVELOPES; ++followerIndex) {
        JsonObject mapping = efSlots.createNestedObject();
        mapping["index"] = followerIndex;
        JsonArray targets = mapping.createNestedArray("slots");
        for (uint8_t slotIndex = 0; slotIndex < NUM_POTS; ++slotIndex) {
            auto it = potToEnvelopeMap.find(slotIndex);
            if (it == potToEnvelopeMap.end()) {
                continue;
            }
            if (it->second.followerIndex != static_cast<int8_t>(followerIndex)) {
                continue;
            }
            targets.add(slotIndex);
        }
        if (targets.size() == 1) {
            mapping["slot"] = targets[0].as<uint8_t>();
        }
    }
}

// Runtime follower state gives a host the live modulation weather, not just the saved setup.
void writeEnvelopeRuntime(JsonObject env) {
    JsonArray routing = env.createNestedArray("routing");
    for (uint8_t i = 0; i < NUM_POTS; ++i) {
        int mapping = -1;
        auto it = potToEnvelopeMap.find(i);
        if (it != potToEnvelopeMap.end()) {
            mapping = it->second.followerIndex;
        }
        routing.add(mapping);
    }

    JsonArray followers = env.createNestedArray("followers");
    for (size_t i = 0; i < envelopeFollowers.size(); ++i) {
        JsonObject follower = followers.createNestedObject();
        follower["index"] = static_cast<uint8_t>(i);
        follower["active"] = envelopeFollowers[i].getActiveState();
        follower["filter"] = envelopeFilterName(envelopeFollowers[i].getFilterType());
        follower["baseline"] = envelopeConfig.baselines[i];
        follower["oversample"] = envelopeFollowers[i].getOversampleCount();
        follower["smoothing"] = envelopeFollowers[i].getSmoothingAlpha();
    }

    uint8_t storedMode = configManager.getMode();
    env["mode"] = storedMode;
    env["mode_name"] = envelopeModeName(storedMode);

    uint8_t storedMethod = configManager.getARGMethod();
    env["arg_method"] = storedMethod;
    env["arg_method_name"] = argMethodName(storedMethod);
    env["arg_enable"] = configManager.getARGEnable();

    JsonObject argPair = env.createNestedObject("arg_pair");
    argPair["a"] = configManager.getEnvelopeA();
    argPair["b"] = configManager.getEnvelopeB();
}

void writeEnvelopeFilterViews(JsonObject env, JsonObject rootFilter) {
    float freq = 0.0f;
    float q = 0.0f;
    ConfigManager::getStorageBackend()->readBytes(EEPROM_FILTER_FREQ, &freq, sizeof(freq));
    ConfigManager::getStorageBackend()->readBytes(EEPROM_FILTER_Q, &q, sizeof(q));
    EnvelopeFollower::FilterType currentFilter = envelopeFollowers.empty()
                                                     ? EnvelopeFollower::LINEAR
                                                     : envelopeFollowers.front().getFilterType();
    JsonObject envFilter = env.createNestedObject("filter");
    envFilter["type"] = envelopeFilterName(currentFilter);
    envFilter["frequency"] = freq;
    envFilter["q"] = q;
    envFilter["idle_floor"] = configManager.getEfIdleFloor();
    env["idle_floor"] = configManager.getEfIdleFloor();

    rootFilter["type"] = envelopeFilterName(currentFilter);
    rootFilter["freq"] = freq;
    rootFilter["q"] = q;
    rootFilter["idle_floor"] = configManager.getEfIdleFloor();
}

void writeRootArgConfig(JsonObject rootArg) {
    uint8_t storedMethod = configManager.getARGMethod();
    rootArg["method"] = argMethodName(storedMethod);
    rootArg["method_index"] = storedMethod;
    rootArg["a"] = configManager.getEnvelopeA();
    rootArg["b"] = configManager.getEnvelopeB();
    rootArg["enable"] = configManager.getARGEnable() != 0;
}

void writeLedConfig(JsonObject led) {
    led["brightness"] = ledManager.getBrightness();
    CRGB color = ledManager.getColor();
    JsonObject colorObj = led.createNestedObject("rgb");
    colorObj["r"] = color.r;
    colorObj["g"] = color.g;
    colorObj["b"] = color.b;
    char hex[8];
    snprintf(hex, sizeof(hex), "#%02X%02X%02X", color.r, color.g, color.b);
    led["color"] = hex;
    led["hex"] = hex;
    led["mode"] = ledModeToString(configManager.getLedMode());
}
} // namespace

// 1. Deprecated compatibility shims kept for older host tooling.
void handleGetAllCommand(const String &command) {
    (void)command;
#ifdef SERIAL_LOGGING
    LOG_PRINTLN("{\"type\":\"response\",\"message\":\"GET_ALL deprecated, use GET_CONFIG\"}");
#endif
}

void handleGetArgMethodCommand(const String &command) {
    (void)command;
    LOG_PRINTLN("{\"type\":\"response\",\"message\":\"get_arg_method deprecated\"}");
}

void handleGetBrownoutsCommand(const String &command) {
    (void)command;
    LOG_PRINTLN("{\"type\":\"response\",\"message\":\"get_brownouts deprecated\"}");
}

// 2. Identity/config export reads.
void handleHelloCommand(const String &command) {
    (void)command;
    LOG_PRINTLN("{\"hello\":\"mn42\"}");
}

void handleGetManifestCommand(const String &command) {
    (void)command;
    StaticJsonDocument<768> doc;
    writeManifestFields(doc.to<JsonObject>());

    if (doc.overflowed()) {
        DiagnosticRecord::recordProtocolError("json_overflow");
        LOG_PRINTLN("{\"type\":\"error\",\"code\":\"json_overflow\",\"scope\":\"GET_MANIFEST\"}");
        return;
    }

    String payload;
    serializeJson(doc, payload);
    LOG_PRINTLN(payload);
}

void handleGetSchemaCommand(const String &command) {
    (void)command;
    LOG_PRINTLN(ConfigManager::makeSchema());
}

void handleGetDiagnosticsCommand(const String &command) {
    (void)command;
    StaticJsonDocument<512> doc;
    DiagnosticRecord::writeJson(doc.to<JsonObject>());

    if (doc.overflowed()) {
        DiagnosticRecord::recordProtocolError("json_overflow");
        LOG_PRINTLN(
            "{\"type\":\"error\",\"code\":\"json_overflow\",\"scope\":\"GET_DIAGNOSTICS\"}");
        return;
    }

    String payload;
    serializeJson(doc, payload);
    LOG_PRINTLN(payload);
}

void handleGetModMatrixCommand(const String &command) {
    (void)command;
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

    JsonArray routes = doc.createNestedArray("routes");
    writePotRoutes(routes);
    writeEfRoutes(routes);
    writeLfoRoutes(routes);

    JsonArray conflicts = doc.createNestedArray("conflicts");
    writeMidiCcConflicts(conflicts);
    writeSlotValueConflicts(conflicts);

    if (doc.overflowed()) {
        DiagnosticRecord::recordProtocolError("json_overflow");
        LOG_PRINTLN("{\"type\":\"error\",\"code\":\"json_overflow\",\"scope\":\"GET_MOD_MATRIX\"}");
        return;
    }

    String payload;
    serializeJson(doc, payload);
    LOG_PRINTLN(payload);
}

// Full config export is intentionally the longest simple handler because it is
// the canonical "describe the current machine state" reply used by hosts.
void handleGetConfigCommand(const String &command) {
    (void)command;
    auto &doc = getConfigDoc;
    doc.clear();

    doc["fw_version"] = FW_VERSION_STR;
    doc["schema_version"] = CONFIG_VERSION;

    JsonArray pots = doc.createNestedArray("pots");
    writePotMappings(pots);

    JsonArray slots = doc.createNestedArray("slots");
    const auto &slotDefs = configManager.getSlots();
    for (uint8_t i = 0; i < NUM_SLOTS; ++i) {
        writeSlotConfig(slots, i, slotDefs[i]);
    }

    JsonArray efSlots = doc.createNestedArray("efSlots");
    writeEfSlotMappings(efSlots);

    JsonObject env = doc.createNestedObject("envelopes");
    writeEnvelopeRuntime(env);

    JsonObject rootFilter = doc.createNestedObject("filter");
    writeEnvelopeFilterViews(env, rootFilter);

    JsonObject rootArg = doc.createNestedObject("arg");
    writeRootArgConfig(rootArg);

    JsonObject led = doc.createNestedObject("led");
    writeLedConfig(led);

    if (doc.overflowed()) {
        DiagnosticRecord::recordProtocolError("json_overflow");
        LOG_PRINTLN("{\"type\":\"error\",\"code\":\"json_overflow\",\"scope\":\"GET_CONFIG\"}");
        return;
    }

    String payload;
    serializeJson(doc, payload);
    LOG_PRINTLN(payload);
    // Start high-rate WebSerial telemetry only after the configurator has successfully hydrated.
    webSerialStreaming = true;
}

// 3. Live runtime inspection reads.
void handleGetClockCommand(const String &command) {
    (void)command;
    const bool externalSignal = midiHandler.hasExternalClockSignal();
    const bool running = midiHandler.isClockRunning();
    const float externalBpm = midiHandler.externalClockBpm();
    const char *source = "idle";
    if (g_followExternalClock && externalSignal) {
        source = "external";
    } else if (g_tappedBPM > 0.0f) {
        source = "internal";
    }
    LOG_PRINTF("{\"type\":\"response\",\"command\":\"GET_CLOCK\",\"follow_external\":%s,"
               "\"clock_out_enabled\":%s,\"tapped_bpm\":%.2f,\"external_bpm\":%.2f,"
               "\"external_signal\":%s,\"running\":%s,\"source\":\"%s\"}\n",
               g_followExternalClock ? "true" : "false", g_clockOutEnabled ? "true" : "false",
               static_cast<double>(g_tappedBPM), static_cast<double>(externalBpm),
               externalSignal ? "true" : "false", running ? "true" : "false", source);
}

void handleGetArpCommand(const String &command) {
    (void)command;
    const Arpeggiator::Shape shape = arpeggiator.getShape();
    const uint8_t slotIndex = arpeggiator.getSlot();
    const MIDISlot &slot = configManager.getSlot(slotIndex);
    LOG_PRINTF("{\"type\":\"response\",\"command\":\"GET_ARP\",\"active\":%s,\"slot\":%u,"
               "\"length_ticks\":%u,\"shape\":%u,\"shape_name\":\"%s\","
               "\"swing_percent\":%u,\"gate_percent\":%u,\"octave_range\":%u,"
               "\"pattern_length\":%u,\"slot_active\":%s,\"slot_type\":\"%s\","
               "\"channel\":%u,\"data1\":%u,\"arp_note\":%u,\"arpNote\":%u}\n",
               arpeggiator.isActive() ? "true" : "false", static_cast<unsigned>(slotIndex),
               static_cast<unsigned>(arpeggiator.getLength()), static_cast<unsigned>(shape),
               arpShapeName(shape),
               static_cast<unsigned>(constrain(arpeggiator.getSwingPercent(), 0.0f, 80.0f)),
               static_cast<unsigned>(constrain(arpeggiator.getGatePercent(), 5.0f, 100.0f)),
               static_cast<unsigned>(arpeggiator.getOctaveRange()),
               static_cast<unsigned>(arpeggiator.getPatternLength()),
               slot.active ? "true" : "false", midiMessageTypeName(slot.type),
               static_cast<unsigned>(slot.midiChannel), static_cast<unsigned>(slot.data1),
               static_cast<unsigned>(slot.arpNote), static_cast<unsigned>(slot.arpNote));
}

void handleGetEfCommand(const String &command) {
    int potIndex = command.substring(7).toInt();
    if (potIndex >= 0 && potIndex < NUM_POTS) {
        int env = -1;
        auto it = potToEnvelopeMap.find(potIndex);
        if (it != potToEnvelopeMap.end()) {
            env = it->second.followerIndex;
        }
#ifdef SERIAL_LOGGING
        LOG_PRINTLN("{\"type\":\"response\",\"message\":\"get_ef deprecated\"}");
#else
        (void)env;
#endif
    } else {
        LOG_PRINTLN("{\"type\":\"response\",\"status\":\"error\"}");
    }
}

void handleGetJitterCommand(const String &command) {
    (void)command;
    LOG_PRINTF(
        "{\"type\":\"response\",\"command\":\"GET_JITTER\",\"depth\":%.3f,\"smoothness\":%.3f}\n",
        static_cast<double>(constrain(g_jitterSettings.depth, 0.0f, 1.0f)),
        static_cast<double>(constrain(g_jitterSettings.smoothness, 0.0f, 1.0f)));
}

void handleGetLedCommand(const String &command) {
    (void)command;
#ifdef SERIAL_LOGGING
    LOG_PRINTLN("{\"type\":\"response\",\"message\":\"get_led deprecated\"}");
#endif
}

void handleGetNoteDynamicsCommand(const String &command) {
    (void)command;
    LOG_PRINTF("{\"type\":\"response\",\"command\":\"GET_NOTE_DYNAMICS\",\"velocity_shift\":%d,"
               "\"change_probability\":%u}\n",
               static_cast<int>(velocityShift), static_cast<unsigned>(changeProbability));
}

void handleGetUsbMidiCommand(const String &command) {
    (void)command;
    LOG_PRINTF("{\"type\":\"response\",\"command\":\"GET_USB_MIDI\",\"usb_midi_out\":%s,"
               "\"rx_count\":%lu,\"tx_count\":%lu,\"clock_ticks\":%lu,"
               "\"clock_running\":%s,\"external_signal\":%s,\"midi_drops\":%lu}\n",
               g_usbMidiOutEnabled ? "true" : "false",
               static_cast<unsigned long>(midiHandler.getRxCount()),
               static_cast<unsigned long>(midiHandler.getTxCount()),
               static_cast<unsigned long>(midiHandler.clockTickCount()),
               midiHandler.isClockRunning() ? "true" : "false",
               midiHandler.hasExternalClockSignal() ? "true" : "false",
               static_cast<unsigned long>(g_systemDiagnostics.midiDropCount));
}

void handleMidiTestCommand(const String &command) {
    (void)command;
    if (!g_usbMidiOutEnabled) {
        configManager.setUsbMidiOutEnabled(true);
    }

    const uint32_t before = midiHandler.getTxCount();
    midiHandler.sendNoteOn(60, 100, 1);
    midiHandler.sendControlChange(1, 64, 1);
    midiHandler.sendNoteOff(60, 0, 1);
    midiHandler.flushUsbMidi();

    LOG_PRINTF("{\"type\":\"response\",\"status\":\"ok\",\"command\":\"MIDI_TEST\","
               "\"usb_midi_out\":%s,\"rx_count\":%lu,\"tx_before\":%lu,\"tx_after\":%lu,"
               "\"clock_ticks\":%lu,\"clock_running\":%s,\"external_signal\":%s,"
               "\"midi_drops\":%lu}\n",
               g_usbMidiOutEnabled ? "true" : "false",
               static_cast<unsigned long>(midiHandler.getRxCount()),
               static_cast<unsigned long>(before),
               static_cast<unsigned long>(midiHandler.getTxCount()),
               static_cast<unsigned long>(midiHandler.clockTickCount()),
               midiHandler.isClockRunning() ? "true" : "false",
               midiHandler.hasExternalClockSignal() ? "true" : "false",
               static_cast<unsigned long>(g_systemDiagnostics.midiDropCount));
}

// 4. Direct live-control writes.
void handleSetArgMethodCommand(const String &command) {
    int method = command.substring(14).toInt();
    if (method >= 0 && method <= static_cast<int>(ARGMethod::XORR)) {
        for (uint8_t slotIndex = 0; slotIndex < NUM_SLOTS; ++slotIndex) {
            MIDISlot &slot = configManager.getSlot(slotIndex);
            slot.arg.method = static_cast<ARGMethod>(method);
            configManager.saveSlot(slotIndex, slot);
        }
        configManager.setARGMethod(static_cast<uint8_t>(method));
        LOG_PRINTLN("{\"type\":\"response\",\"status\":\"ok\"}");
    } else {
        LOG_PRINTLN("{\"type\":\"response\",\"status\":\"error\"}");
    }
}

void handleSetArpCommand(const String &command) {
    int firstComma = command.indexOf(',');
    int secondComma = command.indexOf(',', firstComma + 1);
    int thirdComma = command.indexOf(',', secondComma + 1);
    int fourthComma = command.indexOf(',', thirdComma + 1);
    int fifthComma = command.indexOf(',', fourthComma + 1);
    int sixthComma = command.indexOf(',', fifthComma + 1);
    if (firstComma < 0 || secondComma < 0 || thirdComma < 0 || fourthComma < 0 || fifthComma < 0) {
        LOG_PRINTLN("{\"type\":\"response\",\"status\":\"error\",\"command\":\"SET_ARP\","
                    "\"message\":\"missing values\"}");
        return;
    }

    const uint8_t lengthTicks =
        static_cast<uint8_t>(constrain(command.substring(firstComma + 1, secondComma).toInt(), 1,
                                       static_cast<int>(Arpeggiator::MAX_LENGTH)));
    const uint8_t shape =
        static_cast<uint8_t>(constrain(command.substring(secondComma + 1, thirdComma).toInt(), 0,
                                       static_cast<int>(Arpeggiator::EUCLIDEAN)));
    const float swingPercent =
        constrain(command.substring(thirdComma + 1, fourthComma).toFloat(), 0.0f, 80.0f);
    const float gatePercent =
        constrain(command.substring(fourthComma + 1, fifthComma).toFloat(), 5.0f, 100.0f);
    const int rawOctaveRange = sixthComma < 0
                                   ? command.substring(fifthComma + 1).toInt()
                                   : command.substring(fifthComma + 1, sixthComma).toInt();
    const uint8_t octaveRange = static_cast<uint8_t>(constrain(rawOctaveRange, 0, 3));
    const uint8_t patternLength =
        sixthComma < 0
            ? arpeggiator.getPatternLength()
            : static_cast<uint8_t>(constrain(command.substring(sixthComma + 1).toInt(),
                                             static_cast<int>(Arpeggiator::MIN_PATTERN_LENGTH),
                                             static_cast<int>(Arpeggiator::MAX_PATTERN_LENGTH)));

    arpeggiator.setLength(lengthTicks);
    arpeggiator.setShape(static_cast<Arpeggiator::Shape>(shape));
    arpeggiator.setSwingPercent(swingPercent);
    arpeggiator.setGatePercent(gatePercent);
    arpeggiator.setOctaveRange(octaveRange);
    arpeggiator.setPatternLength(patternLength);
    const bool persisted = persistActiveProfileSnapshot();

    LOG_PRINTF("{\"type\":\"response\",\"status\":\"ok\",\"command\":\"SET_ARP\","
               "\"active\":%s,\"slot\":%u,\"length_ticks\":%u,\"shape\":%u,"
               "\"shape_name\":\"%s\",\"swing_percent\":%u,\"gate_percent\":%u,"
               "\"octave_range\":%u,\"pattern_length\":%u,\"persisted\":%s}\n",
               arpeggiator.isActive() ? "true" : "false",
               static_cast<unsigned>(arpeggiator.getSlot()), static_cast<unsigned>(lengthTicks),
               static_cast<unsigned>(shape), arpShapeName(static_cast<Arpeggiator::Shape>(shape)),
               static_cast<unsigned>(constrain(swingPercent, 0.0f, 80.0f)),
               static_cast<unsigned>(constrain(gatePercent, 5.0f, 100.0f)),
               static_cast<unsigned>(octaveRange),
               static_cast<unsigned>(arpeggiator.getPatternLength()), persisted ? "true" : "false");
}

void handleSetClockCommand(const String &command) {
    int firstComma = command.indexOf(',');
    int secondComma = command.indexOf(',', firstComma + 1);
    int thirdComma = command.indexOf(',', secondComma + 1);
    if (firstComma < 0 || secondComma < 0 || thirdComma < 0) {
        LOG_PRINTLN("{\"type\":\"response\",\"status\":\"error\",\"command\":\"SET_CLOCK\","
                    "\"message\":\"missing values\"}");
        return;
    }

    const bool followExternal = command.substring(firstComma + 1, secondComma).toInt() != 0;
    const bool clockOutEnabled = command.substring(secondComma + 1, thirdComma).toInt() != 0;
    const float tappedBpm = constrain(command.substring(thirdComma + 1).toFloat(), 20.0f, 300.0f);

    g_followExternalClock = followExternal;
    g_clockOutEnabled = clockOutEnabled;
    g_tappedBPM = tappedBpm;
    const bool persisted = persistActiveProfileSnapshot();

    const bool externalSignal = midiHandler.hasExternalClockSignal();
    const bool running = midiHandler.isClockRunning();
    const float externalBpm = midiHandler.externalClockBpm();
    const char *source = "idle";
    if (g_followExternalClock && externalSignal) {
        source = "external";
    } else if (g_tappedBPM > 0.0f) {
        source = "internal";
    }

    LOG_PRINTF("{\"type\":\"response\",\"status\":\"ok\",\"command\":\"SET_CLOCK\","
               "\"follow_external\":%s,\"clock_out_enabled\":%s,\"tapped_bpm\":%.2f,"
               "\"external_bpm\":%.2f,\"external_signal\":%s,\"running\":%s,"
               "\"source\":\"%s\",\"persisted\":%s}\n",
               g_followExternalClock ? "true" : "false", g_clockOutEnabled ? "true" : "false",
               static_cast<double>(g_tappedBPM), static_cast<double>(externalBpm),
               externalSignal ? "true" : "false", running ? "true" : "false", source,
               persisted ? "true" : "false");
}

void handleSetEfCommand(const String &command) {
    int comma = command.indexOf(',');
    if (comma == -1) {
        LOG_PRINTLN("{\"type\":\"response\",\"status\":\"error\"}");
        return;
    }
    int potIndex = command.substring(7, comma).toInt();
    int envIndex = command.substring(comma + 1).toInt();
    if (potIndex >= 0 && potIndex < NUM_POTS && envIndex >= 0 &&
        envIndex < static_cast<int>(envelopeFollowers.size())) {
        MIDISlot &slot = configManager.getSlot(static_cast<uint8_t>(potIndex));
        slot.setEnvelopeFollowerIndex(static_cast<int8_t>(envIndex));
        potToEnvelopeMap[potIndex] = slot.efSettings;
        envelopeFollowers[envIndex].toggleActive(true);
        applyEfSettingsToFollower(envelopeFollowers[envIndex], slot.efSettings,
                                  static_cast<uint8_t>(envIndex));
        configManager.saveEnvelopeSettings(potToEnvelopeMap, envelopeFollowers);
        refreshEfVoicesFromConfig();
        if (persistActiveProfileSnapshot()) {
            LOG_PRINTLN("{\"type\":\"response\",\"status\":\"ok\"}");
        } else {
            LOG_PRINTLN("{\"type\":\"response\",\"status\":\"error\","
                        "\"message\":\"active profile snapshot save failed\"}");
        }
    } else {
        LOG_PRINTLN("{\"type\":\"response\",\"status\":\"error\"}");
    }
}

void handleSetLedCommand(const String &command) {
    int first = command.indexOf(',');
    int second = command.indexOf(',', first + 1);
    int third = command.indexOf(',', second + 1);
    if (first == -1 || second == -1 || third == -1) {
        LOG_PRINTLN("{\"type\":\"response\",\"status\":\"error\"}");
        return;
    }
    int brightness = command.substring(8, first).toInt();
    int r = command.substring(first + 1, second).toInt();
    int g = command.substring(second + 1, third).toInt();
    int b = command.substring(third + 1).toInt();
    if (brightness >= 0 && brightness <= 255 && r >= 0 && r <= 255 && g >= 0 && g <= 255 &&
        b >= 0 && b <= 255) {
        CRGB color(r, g, b);
        brightness = std::min<int>(brightness, BoardPowerProfile::kLedBrightnessCap);
        ledManager.setBrightness(static_cast<uint8_t>(brightness));
        ledManager.setColor(color);
        configManager.saveLEDSettings(static_cast<uint8_t>(brightness), color);
        if (persistActiveProfileSnapshot()) {
            LOG_PRINTLN("{\"type\":\"response\",\"status\":\"ok\"}");
        } else {
            LOG_PRINTLN("{\"type\":\"response\",\"status\":\"error\","
                        "\"message\":\"active profile snapshot save failed\"}");
        }
    } else {
        LOG_PRINTLN("{\"type\":\"response\",\"status\":\"error\"}");
    }
}

void handleSetJitterCommand(const String &command) {
    int firstComma = command.indexOf(',');
    int secondComma = command.indexOf(',', firstComma + 1);
    if (firstComma < 0 || secondComma < 0) {
        LOG_PRINTLN("{\"type\":\"response\",\"status\":\"error\",\"command\":\"SET_JITTER\","
                    "\"message\":\"missing values\"}");
        return;
    }

    const float depth =
        constrain(command.substring(firstComma + 1, secondComma).toFloat(), 0.0f, 1.0f);
    const float smoothness = constrain(command.substring(secondComma + 1).toFloat(), 0.0f, 1.0f);
    g_jitterSettings.depth = depth;
    g_jitterSettings.smoothness = smoothness;
    g_jitterRemoteControlActive = true;
    g_jitterDepthLatched = false;
    g_jitterSmoothnessLatched = false;
    const bool persisted = persistActiveProfileSnapshot();
    LOG_PRINTF("{\"type\":\"response\",\"status\":\"ok\",\"command\":\"SET_JITTER\",\"depth\":%.3f,"
               "\"smoothness\":%.3f,\"persisted\":%s}\n",
               static_cast<double>(depth), static_cast<double>(smoothness),
               persisted ? "true" : "false");
}

void handleSetPotCommand(const String &command) {
    int firstComma = command.indexOf(',');
    int lastComma = command.lastIndexOf(',');
    if (firstComma == -1 || lastComma == -1 || firstComma == lastComma) {
        LOG_PRINTLN("{\"type\":\"response\",\"status\":\"error\",\"message\":\"Malformed SET_POT "
                    "command\"}");
        return;
    }
    int potIndex = command.substring(8, firstComma).toInt();
    int channel = command.substring(firstComma + 1, lastComma).toInt();
    int ccNumber = command.substring(lastComma + 1).toInt();
    if (potIndex >= 0 && potIndex < NUM_POTS && channel >= 1 && channel <= 16 && ccNumber >= 0 &&
        ccNumber <= 127) {
        configManager.setPotChannel(potIndex, channel);
        configManager.setPotCCNumber(potIndex, ccNumber);
        potentiometerManager.setChannel(potIndex, channel);
        potentiometerManager.setCCNumber(potIndex, ccNumber);
        if (static_cast<size_t>(potIndex) < potChannels.size()) {
            potChannels[potIndex] = channel;
        }
        configManager.saveConfiguration();
        LOG_PRINTLN(
            "{\"type\":\"response\",\"status\":\"ok\",\"message\":\"Pot configuration updated!\"}");
    } else {
        LOG_PRINTLN("{\"type\":\"response\",\"status\":\"error\",\"message\":\"Invalid values for "
                    "SET_POT\"}");
    }
}

void handleSetNoteDynamicsCommand(const String &command) {
    int firstComma = command.indexOf(',');
    int secondComma = command.indexOf(',', firstComma + 1);
    if (firstComma < 0 || secondComma < 0) {
        LOG_PRINTLN("{\"type\":\"response\",\"status\":\"error\",\"command\":\"SET_NOTE_DYNAMICS\","
                    "\"message\":\"missing values\"}");
        return;
    }

    const int velocity = constrain(command.substring(firstComma + 1, secondComma).toInt(), -64, 63);
    const int probability = constrain(command.substring(secondComma + 1).toInt(), 0, 100);
    velocityShift = static_cast<int8_t>(velocity);
    changeProbability = static_cast<uint8_t>(probability);
    g_noteDynamicsRemoteControlActive = true;
    g_noteDynamicsShiftLatched = false;
    g_noteDynamicsProbabilityLatched = false;
    const bool persisted = persistActiveProfileSnapshot();
    LOG_PRINTF("{\"type\":\"response\",\"status\":\"ok\",\"command\":\"SET_NOTE_DYNAMICS\","
               "\"velocity_shift\":%d,\"change_probability\":%u,\"persisted\":%s}\n",
               velocity, static_cast<unsigned>(changeProbability), persisted ? "true" : "false");
}

void handleSetUsbMidiCommand(const String &command) {
    int comma = command.indexOf(',');
    if (comma < 0) {
        LOG_PRINTLN("{\"type\":\"response\",\"status\":\"error\",\"command\":\"SET_USB_MIDI\","
                    "\"message\":\"missing value\"}");
        return;
    }

    String valueText = command.substring(comma + 1);
    valueText.trim();
    configManager.setUsbMidiOutEnabled(valueText.toInt() != 0);
    LOG_PRINTF("{\"type\":\"response\",\"status\":\"ok\",\"command\":\"SET_USB_MIDI\","
               "\"usb_midi_out\":%s}\n",
               g_usbMidiOutEnabled ? "true" : "false");
}

void handleSetSlotValueCommand(const String &command) {
    int firstComma = command.indexOf(',');
    int lastComma = command.lastIndexOf(',');
    if (firstComma == -1 || lastComma == -1 || firstComma == lastComma) {
        LOG_PRINTLN("{\"type\":\"response\",\"status\":\"error\"}");
        return;
    }

    int slotIndex = command.substring(firstComma + 1, lastComma).toInt();
    int midiValue = command.substring(lastComma + 1).toInt();
    if (slotIndex < 0 || slotIndex >= NUM_SLOTS || midiValue < 0 || midiValue > 127) {
        LOG_PRINTLN("{\"type\":\"response\",\"status\":\"error\"}");
        return;
    }

    potentiometerManager.injectMidiValue(static_cast<uint8_t>(slotIndex),
                                         static_cast<uint8_t>(midiValue));
    LOG_PRINTLN("{\"type\":\"response\",\"status\":\"ok\"}");
}
} // namespace ProtocolSimpleHandlers
