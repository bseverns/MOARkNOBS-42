#include "protocol/ProfileSetHandler.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <array>
#include <cctype>
#include <cmath>

#include "Arpeggiator.h"
#include "ConfigManager.h"
#include "EnvelopeFollower.h"
#include "Globals.h"
#include "LFO/LFOManager.h"
#include "Log.h"
#include "Modes.h"
#include "UI.h"

// ProfileSetHandler.cpp is the structured `SET_PROFILE` patching layer.
//
// Reading order:
// 1. small clamp/parse helpers for the profile JSON shape
// 2. one command entry point that merges sparse host patches onto a captured
//    snapshot, persists the result, and reapplies it when editing the active slot

namespace {
struct ProfileSetRequest {
    uint8_t id = 0;
    String payload;
    bool chunked = false;
};

constexpr size_t kMaxProfilePatchPayloadBytes = 12288;
constexpr uint16_t kMaxProfilePatchChunks = 256;

struct ProfileChunkState {
    bool active = false;
    uint8_t id = 0;
    uint16_t total = 0;
    uint16_t nextSeq = 0;
    String payload;

    void reset() {
        active = false;
        id = 0;
        total = 0;
        nextSeq = 0;
        payload = "";
    }
};

ProfileChunkState profileChunkState;

void logProfileSetError(const char *message) {
    LOG_PRINTF("{\"type\":\"error\",\"code\":\"bad_request\",\"command\":\"SET_PROFILE\","
               "\"message\":\"%s\"}\n",
               message ? message : "Profile update rejected");
}

void logProfileSetError(const String &message) { logProfileSetError(message.c_str()); }

void logProfileJsonParseError(const ProfileSetRequest &request, const char *reason) {
    const unsigned len = static_cast<unsigned>(request.payload.length());
    const unsigned firstCode = len > 0 ? static_cast<unsigned>(request.payload.charAt(0)) : 0U;
    const unsigned lastCode =
        len > 0 ? static_cast<unsigned>(request.payload.charAt(request.payload.length() - 1)) : 0U;
    LOG_PRINTF("{\"type\":\"error\",\"code\":\"bad_request\",\"command\":\"SET_PROFILE\","
               "\"message\":\"Profile JSON did not parse\",\"reason\":\"%s\","
               "\"source\":\"%s\",\"payload_length\":%u,\"first_code\":%u,\"last_code\":%u}\n",
               reason ? reason : "unknown", request.chunked ? "chunked" : "direct", len, firstCode,
               lastCode);
}

uint8_t clampedU8(JsonObject obj, const char *key, int minValue, int maxValue, uint8_t fallback) {
    if (!obj.containsKey(key)) {
        return fallback;
    }
    return static_cast<uint8_t>(constrain(obj[key].as<int>(), minValue, maxValue));
}

uint16_t clampedU16(JsonObject obj, const char *key, int minValue, int maxValue,
                    uint16_t fallback) {
    if (!obj.containsKey(key)) {
        return fallback;
    }
    return static_cast<uint16_t>(constrain(obj[key].as<int>(), minValue, maxValue));
}

int readIntField(JsonObject obj, const char *primary, const char *alternate, int fallback) {
    if (!obj.isNull() && obj.containsKey(primary)) {
        return obj[primary].as<int>();
    }
    if (!obj.isNull() && alternate && obj.containsKey(alternate)) {
        return obj[alternate].as<int>();
    }
    return fallback;
}

uint8_t clampedU8(JsonObject obj, const char *primary, const char *alternate, int minValue,
                  int maxValue, uint8_t fallback) {
    return static_cast<uint8_t>(
        constrain(readIntField(obj, primary, alternate, fallback), minValue, maxValue));
}

String invalidRouteFieldMessage(size_t routeIndex, const char *field, const char *detail) {
    String message = "Route ";
    message += String(static_cast<unsigned>(routeIndex));
    message += " ";
    message += field ? field : "field";
    message += " ";
    message += detail ? detail : "is invalid";
    return message;
}

bool equalsIgnoreCase(const char *lhs, const char *rhs) {
    if (!lhs || !rhs) {
        return false;
    }
    while (*lhs && *rhs) {
        if (tolower(static_cast<unsigned char>(*lhs)) !=
            tolower(static_cast<unsigned char>(*rhs))) {
            return false;
        }
        ++lhs;
        ++rhs;
    }
    return *lhs == '\0' && *rhs == '\0';
}

bool parseMIDITypeLabel(const char *label, MIDIMessageType &type) {
    if (!label) {
        return false;
    }
    struct Entry {
        const char *legacy;
        const char *canonical;
        const char *alt;
        MIDIMessageType value;
    };
    static constexpr Entry kMap[] = {
        {"OFF", "OFF", nullptr, MIDIMessageType::OFF},
        {"UNKNOWN", "UNKNOWN", nullptr, MIDIMessageType::OFF},
        {"CC", "CC", nullptr, MIDIMessageType::CC},
        {"Note", "NOTE", nullptr, MIDIMessageType::Note},
        {"PitchBend", "PITCH_BEND", "PITCHBEND", MIDIMessageType::PitchBend},
        {"ProgramChange", "PROGRAM", "PROGRAM_CHANGE", MIDIMessageType::ProgramChange},
        {"Aftertouch", "AFTERTOUCH", nullptr, MIDIMessageType::Aftertouch},
        {"ModWheel", "MOD_WHEEL", "MODWHEEL", MIDIMessageType::ModWheel},
        {"NRPN", "NRPN", nullptr, MIDIMessageType::NRPN},
        {"RPN", "RPN", nullptr, MIDIMessageType::RPN},
        {"SysEx", "SYSEX", "SYS_EX", MIDIMessageType::SysEx},
    };
    for (const auto &entry : kMap) {
        if ((entry.legacy && equalsIgnoreCase(label, entry.legacy)) ||
            (entry.canonical && equalsIgnoreCase(label, entry.canonical)) ||
            (entry.alt && equalsIgnoreCase(label, entry.alt))) {
            type = entry.value;
            return true;
        }
    }
    return false;
}

bool parseProfileSlotType(JsonObject slot, MIDIMessageType &type) {
    JsonVariant typeValue = slot["type"];
    if (!typeValue.isNull()) {
        if (typeValue.is<const char *>()) {
            return parseMIDITypeLabel(typeValue.as<const char *>(), type);
        }
        const int raw = typeValue.as<int>();
        if (raw >= static_cast<int>(MIDIMessageType::OFF) &&
            raw <= static_cast<int>(MIDIMessageType::SysEx)) {
            type = static_cast<MIDIMessageType>(raw);
            return true;
        }
        return false;
    }
    if (slot.containsKey("type_name")) {
        return parseMIDITypeLabel(slot["type_name"].as<const char *>(), type);
    }
    if (slot.containsKey("schema_name")) {
        return parseMIDITypeLabel(slot["schema_name"].as<const char *>(), type);
    }
    return false;
}

bool parseProfileEf(JsonObject obj, ProfileEfSettings &out) {
    if (obj.isNull()) {
        return false;
    }
    if (obj.containsKey("mode")) {
        int raw = obj["mode"].as<int>();
        out.mode = static_cast<uint8_t>(
            constrain(raw, 0, static_cast<int>(EnvelopeFollower::EFMode::Follower)));
    }
    if (obj.containsKey("auto_baseline")) {
        out.autoBaseline = obj["auto_baseline"].as<bool>() ? 1 : 0;
    }
    if (obj.containsKey("auto_gain")) {
        out.autoGain = obj["auto_gain"].as<bool>() ? 1 : 0;
    }
    if (obj.containsKey("attack_ms")) {
        out.attackMs = clampedU16(obj, "attack_ms", static_cast<int>(EF_TIME_MIN_MS),
                                  static_cast<int>(EF_TIME_MAX_MS), out.attackMs);
    }
    if (obj.containsKey("release_ms")) {
        out.releaseMs = clampedU16(obj, "release_ms", static_cast<int>(EF_TIME_MIN_MS),
                                   static_cast<int>(EF_TIME_MAX_MS), out.releaseMs);
    }
    if (obj.containsKey("rms_ms")) {
        out.rmsWindowMs = clampedU16(obj, "rms_ms", static_cast<int>(EF_TIME_MIN_MS),
                                     static_cast<int>(EF_TIME_MAX_MS), out.rmsWindowMs);
    }
    if (obj.containsKey("baseline_tau_ms")) {
        out.baselineTauMs = clampedU16(obj, "baseline_tau_ms", static_cast<int>(EF_TIME_MIN_MS),
                                       static_cast<int>(EF_TIME_MAX_MS), out.baselineTauMs);
    }
    if (obj.containsKey("gain_tau_ms")) {
        out.gainTauMs = clampedU16(obj, "gain_tau_ms", static_cast<int>(EF_TIME_MIN_MS),
                                   static_cast<int>(EF_TIME_MAX_MS), out.gainTauMs);
    }
    if (obj.containsKey("gate_threshold")) {
        out.gateThreshold = clampedU8(obj, "gate_threshold", 0, 127, out.gateThreshold);
    }
    if (obj.containsKey("gate_hysteresis")) {
        out.gateHysteresis = clampedU8(obj, "gate_hysteresis", 0, 127, out.gateHysteresis);
    }
    if (obj.containsKey("activity_threshold")) {
        out.activityThreshold = clampedU8(obj, "activity_threshold", 0, 127, out.activityThreshold);
    }
    if (obj.containsKey("gain_target")) {
        out.gainTarget = clampedU8(obj, "gain_target", 0, 127, out.gainTarget);
    }
    if (obj.containsKey("destination_mode")) {
        out.destinationMode =
            clampedU8(obj, "destination_mode", 0, static_cast<int>(EfDestinationMode::Centered),
                      out.destinationMode);
    }
    return true;
}

bool parseProfileSetRequest(const String &command, ProfileSetRequest &request) {
    int firstComma = command.indexOf(',');
    int secondComma = command.indexOf(',', firstComma + 1);
    if (firstComma < 0 || secondComma < 0) {
        return false;
    }

    request.id = static_cast<uint8_t>(command.substring(firstComma + 1, secondComma).toInt());
    if (request.id >= NUM_PROFILES) {
        return false;
    }

    request.payload = command.substring(secondComma + 1);
    request.payload.trim();
    request.chunked = false;
    return request.payload.length() > 0;
}

bool parseProfileSetChunkRequest(const String &command, ProfileSetRequest &request,
                                 bool &complete) {
    complete = false;
    int firstComma = command.indexOf(',');
    int secondComma = command.indexOf(',', firstComma + 1);
    int thirdComma = command.indexOf(',', secondComma + 1);
    int fourthComma = command.indexOf(',', thirdComma + 1);
    if (firstComma < 0 || secondComma < 0 || thirdComma < 0 || fourthComma < 0) {
        profileChunkState.reset();
        return false;
    }

    const int rawId = command.substring(firstComma + 1, secondComma).toInt();
    const int rawSeq = command.substring(secondComma + 1, thirdComma).toInt();
    const int rawTotal = command.substring(thirdComma + 1, fourthComma).toInt();
    if (rawId < 0 || rawId >= NUM_PROFILES || rawSeq < 0 || rawTotal <= 0 ||
        rawTotal > kMaxProfilePatchChunks) {
        profileChunkState.reset();
        return false;
    }

    const uint8_t id = static_cast<uint8_t>(rawId);
    const uint16_t seq = static_cast<uint16_t>(rawSeq);
    const uint16_t total = static_cast<uint16_t>(rawTotal);
    const String chunk = command.substring(fourthComma + 1);

    if (seq == 0) {
        profileChunkState.reset();
        profileChunkState.active = true;
        profileChunkState.id = id;
        profileChunkState.total = total;
        profileChunkState.nextSeq = 0;
        const size_t estimatedBytes = static_cast<size_t>(total) * 80U;
        profileChunkState.payload.reserve(estimatedBytes < kMaxProfilePatchPayloadBytes
                                              ? estimatedBytes
                                              : kMaxProfilePatchPayloadBytes);
    }

    if (!profileChunkState.active || profileChunkState.id != id ||
        profileChunkState.total != total || profileChunkState.nextSeq != seq) {
        profileChunkState.reset();
        return false;
    }

    if (profileChunkState.payload.length() + chunk.length() > kMaxProfilePatchPayloadBytes) {
        profileChunkState.reset();
        return false;
    }

    profileChunkState.payload += chunk;
    profileChunkState.nextSeq++;
    if (profileChunkState.nextSeq < profileChunkState.total) {
        return true;
    }

    request.id = profileChunkState.id;
    request.payload = profileChunkState.payload;
    request.payload.trim();
    request.chunked = true;
    profileChunkState.reset();
    complete = request.payload.length() > 0;
    return complete;
}

bool parseProfilePayloadDocument(const String &payload, StaticJsonDocument<12288> &doc,
                                 DeserializationError &err) {
    err = deserializeJson(doc, payload);
    return !err;
}

void applyArpProfilePatch(JsonObject root, ProfileData &profile) {
    if (!root.containsKey("arp")) {
        return;
    }

    JsonObject arp = root["arp"].as<JsonObject>();
    if (arp.containsKey("length_ticks")) {
        profile.arp.lengthTicks = clampedU8(arp, "length_ticks", 1, 255, profile.arp.lengthTicks);
    }
    if (arp.containsKey("shape")) {
        profile.arp.shape = clampedU8(arp, "shape", 0, 255, profile.arp.shape);
    }
    if (arp.containsKey("swing_percent")) {
        profile.arp.swingPercent = clampedU8(arp, "swing_percent", 0, 80, profile.arp.swingPercent);
    }
    if (arp.containsKey("gate_percent")) {
        profile.arp.gatePercent = clampedU8(arp, "gate_percent", 5, 100, profile.arp.gatePercent);
    }
    if (arp.containsKey("octave_range")) {
        profile.arp.octaveRange = clampedU8(arp, "octave_range", 0, 3, profile.arp.octaveRange);
    }
    if (arp.containsKey("pattern_length")) {
        profile.arp.patternLength =
            clampedU8(arp, "pattern_length", Arpeggiator::MIN_PATTERN_LENGTH,
                      Arpeggiator::MAX_PATTERN_LENGTH, profile.arp.patternLength);
    }
}

void applyLedProfilePatch(JsonObject root, ProfileData &profile) {
    if (!root.containsKey("led")) {
        return;
    }

    JsonObject led = root["led"].as<JsonObject>();
    if (led.containsKey("brightness")) {
        profile.led.brightness = clampedU8(led, "brightness", 0, 255, profile.led.brightness);
    }
    if (led.containsKey("rgb")) {
        JsonObject rgb = led["rgb"].as<JsonObject>();
        profile.led.r = clampedU8(rgb, "r", 0, 255, profile.led.r);
        profile.led.g = clampedU8(rgb, "g", 0, 255, profile.led.g);
        profile.led.b = clampedU8(rgb, "b", 0, 255, profile.led.b);
    }
}

void applyClockProfilePatch(JsonObject root, ProfileData &profile) {
    if (!root.containsKey("clock")) {
        return;
    }

    JsonObject clock = root["clock"].as<JsonObject>();
    if (clock.containsKey("follow_external")) {
        profile.clock.followExternalClock = clock["follow_external"].as<bool>() ? 1 : 0;
    }
    if (clock.containsKey("clock_out_enabled")) {
        profile.clock.clockOutEnabled = clock["clock_out_enabled"].as<bool>() ? 1 : 0;
    }
    if (clock.containsKey("tapped_bpm")) {
        const float tappedBpm = clock["tapped_bpm"].as<float>();
        profile.clock.tappedBpm = std::isfinite(tappedBpm) ? constrain(tappedBpm, 20.0f, 300.0f)
                                                           : profile.clock.tappedBpm;
    }
}

void applyNoteDynamicsProfilePatch(JsonObject root, ProfileData &profile) {
    if (!root.containsKey("note_dynamics")) {
        return;
    }

    JsonObject noteDynamics = root["note_dynamics"].as<JsonObject>();
    if (noteDynamics.containsKey("velocity_shift")) {
        profile.noteDynamics.velocityShift =
            static_cast<int8_t>(constrain(noteDynamics["velocity_shift"].as<int>(), -64, 63));
    }
    if (noteDynamics.containsKey("change_probability")) {
        profile.noteDynamics.changeProbability = clampedU8(
            noteDynamics, "change_probability", 0, 100, profile.noteDynamics.changeProbability);
    }
}

void applyJitterProfilePatch(JsonObject root, ProfileData &profile) {
    if (!root.containsKey("jitter")) {
        return;
    }

    JsonObject jitter = root["jitter"].as<JsonObject>();
    if (jitter.containsKey("depth")) {
        const float depth = jitter["depth"].as<float>();
        profile.jitter.depth =
            std::isfinite(depth) ? constrain(depth, 0.0f, 1.0f) : profile.jitter.depth;
    }
    if (jitter.containsKey("smoothness")) {
        const float smoothness = jitter["smoothness"].as<float>();
        profile.jitter.smoothness = std::isfinite(smoothness) ? constrain(smoothness, 0.0f, 1.0f)
                                                              : profile.jitter.smoothness;
    }
}

void applyLfoProfilePatch(JsonObject root, ProfileData &profile) {
    if (!root.containsKey("lfos")) {
        return;
    }

    JsonArray lfos = root["lfos"].as<JsonArray>();
    for (JsonObject lfo : lfos) {
        uint8_t index = static_cast<uint8_t>(lfo["index"].as<int>());
        if (index >= PROFILE_LFO_COUNT) {
            continue;
        }
        if (lfo.containsKey("shape")) {
            profile.lfos[index].shape = static_cast<uint8_t>(lfo["shape"].as<int>());
        }
        if (lfo.containsKey("frequency_hz")) {
            profile.lfos[index].frequencyHz = lfo["frequency_hz"].as<float>();
        }
        if (lfo.containsKey("depth")) {
            profile.lfos[index].depth = lfo["depth"].as<float>();
        }
        if (lfo.containsKey("bipolar")) {
            profile.lfos[index].bipolar = lfo["bipolar"].as<bool>() ? 1 : 0;
        }
        if (lfo.containsKey("sync")) {
            profile.lfos[index].syncEnabled = lfo["sync"].as<bool>() ? 1 : 0;
        }
        if (lfo.containsKey("sync_ratio")) {
            profile.lfos[index].syncRatio = static_cast<uint8_t>(lfo["sync_ratio"].as<int>());
        }
    }
}

bool parseProfileRoute(JsonObject route, size_t routeIndex, ProfileLfoRoute &out, String &error) {
    const int rawType = readIntField(route, "type", nullptr, 0);
    if (rawType < 0 || rawType > static_cast<int>(LFOManager::Route::Type::SlotValue)) {
        error = invalidRouteFieldMessage(routeIndex, "type", "is out of range");
        return false;
    }
    out.type = static_cast<uint8_t>(rawType);

    const int rawLfo = readIntField(route, "lfo", "lfoIndex", 0);
    if (rawLfo < 0 || rawLfo >= PROFILE_LFO_COUNT) {
        error = invalidRouteFieldMessage(routeIndex, "lfo index", "is out of range");
        return false;
    }
    out.lfoIndex = static_cast<uint8_t>(rawLfo);

    const float rawDepth = route.containsKey("depth") ? route["depth"].as<float>() : 1.0f;
    out.depth = std::isfinite(rawDepth) ? constrain(rawDepth, 0.0f, 1.0f) : 1.0f;

    const int rawAmount = readIntField(route, "amount", nullptr, 100);
    out.amount = static_cast<int8_t>(constrain(rawAmount, -100, 100));

    const int rawMin = readIntField(route, "min", "minValue", 0);
    const int rawMax = readIntField(route, "max", "maxValue", 127);
    out.minValue = static_cast<uint8_t>(constrain(rawMin, 0, 127));
    out.maxValue = static_cast<uint8_t>(constrain(rawMax, 0, 127));
    if (out.minValue > out.maxValue) {
        const uint8_t swapped = out.minValue;
        out.minValue = out.maxValue;
        out.maxValue = swapped;
    }

    const auto routeType = static_cast<LFOManager::Route::Type>(out.type);
    switch (routeType) {
    case LFOManager::Route::Type::Internal: {
        const int rawTarget = readIntField(route, "target", nullptr, 0);
        if (rawTarget < 0 || rawTarget > static_cast<int>(LFOInternalTarget::JitterSmoothness)) {
            error = invalidRouteFieldMessage(routeIndex, "internal target", "is out of range");
            return false;
        }
        out.target = static_cast<uint8_t>(rawTarget);
        out.channel = 1;
        out.ccMsb = 0;
        out.ccLsb = 32;
        return true;
    }
    case LFOManager::Route::Type::SlotValue: {
        const int rawSlot = readIntField(route, "slot", "target", 0);
        if (rawSlot < 0 || rawSlot >= NUM_SLOTS) {
            error = invalidRouteFieldMessage(routeIndex, "slot index", "is out of range");
            return false;
        }
        out.target = static_cast<uint8_t>(rawSlot);
        out.channel = 1;
        out.ccMsb = 0;
        out.ccLsb = 32;
        return true;
    }
    case LFOManager::Route::Type::MidiCC7:
    case LFOManager::Route::Type::MidiCC14: {
        const int rawChannel = readIntField(route, "channel", nullptr, 1);
        if (rawChannel < 1 || rawChannel > 16) {
            error = invalidRouteFieldMessage(routeIndex, "MIDI channel", "is out of range");
            return false;
        }
        const int rawCcMsb = readIntField(route, "cc_msb", "cc", 0);
        if (rawCcMsb < 0 || rawCcMsb > 127) {
            error = invalidRouteFieldMessage(routeIndex, "CC MSB", "is out of range");
            return false;
        }
        out.channel = static_cast<uint8_t>(rawChannel);
        out.ccMsb = static_cast<uint8_t>(rawCcMsb);
        out.target = 0;
        out.ccLsb = 32;
        if (routeType == LFOManager::Route::Type::MidiCC14) {
            const int rawCcLsb = readIntField(route, "cc_lsb", nullptr, 32);
            if (rawCcLsb < 0 || rawCcLsb > 127) {
                error = invalidRouteFieldMessage(routeIndex, "CC LSB", "is out of range");
                return false;
            }
            out.ccLsb = static_cast<uint8_t>(rawCcLsb);
        }
        return true;
    }
    case LFOManager::Route::Type::Osc:
        out.target = 0;
        out.channel = 1;
        out.ccMsb = 0;
        out.ccLsb = 32;
        return true;
    }
    error = invalidRouteFieldMessage(routeIndex, "type", "is unsupported");
    return false;
}

bool applyRouteProfilePatch(JsonObject root, ProfileData &profile, String &error) {
    if (!root.containsKey("routes")) {
        return true;
    }
    if (!root["routes"].is<JsonArray>()) {
        error = "routes must be an array";
        return false;
    }

    JsonArray routes = root["routes"].as<JsonArray>();
    if (routes.size() > PROFILE_MAX_ROUTES) {
        error = "routes exceeds max entries";
        return false;
    }

    std::array<ProfileLfoRoute, PROFILE_MAX_ROUTES> parsedRoutes{};
    uint8_t parsedCount = 0;
    size_t routeIndex = 0;
    for (JsonVariant routeValue : routes) {
        if (!routeValue.is<JsonObject>()) {
            error = invalidRouteFieldMessage(routeIndex, "entry", "must be an object");
            return false;
        }
        if (!parseProfileRoute(routeValue.as<JsonObject>(), routeIndex, parsedRoutes[parsedCount],
                               error)) {
            return false;
        }
        ++parsedCount;
        ++routeIndex;
    }

    profile.routeCount = parsedCount;
    for (uint8_t i = 0; i < PROFILE_MAX_ROUTES; ++i) {
        profile.routes[i] = i < parsedCount ? parsedRoutes[i] : ProfileLfoRoute{};
    }
    return true;
}

void applySlotProfilePatch(JsonObject root, ProfileData &profile, bool applyLiveSlots) {
    if (!root.containsKey("slots")) {
        return;
    }

    JsonArray slots = root["slots"].as<JsonArray>();
    for (JsonObject slot : slots) {
        uint8_t index = static_cast<uint8_t>(slot["index"].as<int>());
        if (index >= NUM_SLOTS) {
            continue;
        }
        MIDISlot *liveSlot = applyLiveSlots ? &configManager.getSlot(index) : nullptr;
        bool liveSlotChanged = false;
        if (slot.containsKey("channel")) {
            profile.slots[index].midiChannel =
                clampedU8(slot, "channel", 1, 16, profile.slots[index].midiChannel);
            if (liveSlot) {
                liveSlot->midiChannel = profile.slots[index].midiChannel;
                liveSlotChanged = true;
            }
        }
        if (slot.containsKey("midiChannel")) {
            profile.slots[index].midiChannel =
                clampedU8(slot, "midiChannel", 1, 16, profile.slots[index].midiChannel);
            if (liveSlot) {
                liveSlot->midiChannel = profile.slots[index].midiChannel;
                liveSlotChanged = true;
            }
        }
        MIDIMessageType midiType = liveSlot ? liveSlot->type : MIDIMessageType::OFF;
        if (parseProfileSlotType(slot, midiType)) {
            if (liveSlot) {
                liveSlot->type = midiType;
                liveSlotChanged = true;
            }
        }
        const bool hasExplicitArpNote = slot.containsKey("arpNote") || slot.containsKey("arp_note");
        if (slot.containsKey("data1") || slot.containsKey("cc")) {
            const uint8_t data1 =
                clampedU8(slot, "data1", "cc", 0, 127, liveSlot ? liveSlot->data1 : 0);
            if (liveSlot) {
                liveSlot->data1 = data1;
                if (!hasExplicitArpNote && liveSlot->type == MIDIMessageType::Note) {
                    liveSlot->arpNote = data1;
                }
                liveSlotChanged = true;
            }
        }
        if (!slot.containsKey("data1") && !slot.containsKey("cc") && hasExplicitArpNote) {
            const uint8_t note =
                clampedU8(slot, "arpNote", "arp_note", 0, 127, liveSlot ? liveSlot->data1 : 0);
            if (liveSlot) {
                liveSlot->data1 = note;
                liveSlot->arpNote = note;
                liveSlotChanged = true;
            }
        } else if (hasExplicitArpNote) {
            const uint8_t note =
                clampedU8(slot, "arpNote", "arp_note", 0, 127, liveSlot ? liveSlot->arpNote : 0);
            if (liveSlot) {
                liveSlot->arpNote = note;
                liveSlotChanged = true;
            }
        }
        if (slot.containsKey("active")) {
            if (liveSlot) {
                liveSlot->active = slot["active"].as<bool>();
                liveSlotChanged = true;
            }
        }
        if (slot.containsKey("ef")) {
            JsonObject ef = slot["ef"].as<JsonObject>();
            parseProfileEf(ef, profile.slots[index].ef);
        }
        if (liveSlotChanged) {
            configManager.saveSlot(index, *liveSlot);
        }
    }
}

bool persistPatchedProfile(uint8_t id, ProfileData &profile, bool &activeApplied) {
    ProfileModulationExtension modulation{};
    if (id == g_activeProfile) {
        modulation = captureProfileModulation();
    } else if (!configManager.loadProfileModulation(id, modulation)) {
        // A profile created before schema 8 shared the live slot modulation.
        modulation = captureProfileModulation();
    }
    if (!configManager.saveProfileSettings(id, profile) ||
        !configManager.saveProfileModulation(id, modulation)) {
        return false;
    }

    activeApplied = false;
    if (id != g_activeProfile) {
        return true;
    }

    applyProfileSnapshot(profile, true);
    applyProfileModulation(modulation, true);
    activeApplied = true;
    return true;
}

ProfileData buildPatchedProfileBaseline(uint8_t id) {
    if (id == g_activeProfile) {
        return captureProfileSnapshot();
    }

    ProfileData profile{};
    configManager.loadProfileSettings(id, profile);
    return profile;
}
} // namespace

void handleSetProfilePayloadCommand(const String &command) {
    // Merge incoming JSON onto a captured snapshot so callers may send sparse profile patches
    // instead of a full profile document every time.
    ProfileSetRequest request;
    if (command.startsWith("SET_PROFILE_CHUNK")) {
        bool complete = false;
        if (!parseProfileSetChunkRequest(command, request, complete)) {
            logProfileSetError("Malformed SET_PROFILE_CHUNK frame");
            return;
        }
        if (!complete) {
            return;
        }
    } else {
        if (!parseProfileSetRequest(command, request)) {
            logProfileSetError("Malformed SET_PROFILE request");
            return;
        }
    }

    StaticJsonDocument<12288> doc;
    DeserializationError jsonError;
    if (!parseProfilePayloadDocument(request.payload, doc, jsonError)) {
        logProfileJsonParseError(request, jsonError.c_str());
        return;
    }

    ProfileData profile = buildPatchedProfileBaseline(request.id);
    JsonObject root = doc.as<JsonObject>();
    applyArpProfilePatch(root, profile);
    applyLedProfilePatch(root, profile);
    applyClockProfilePatch(root, profile);
    applyNoteDynamicsProfilePatch(root, profile);
    applyJitterProfilePatch(root, profile);
    applyLfoProfilePatch(root, profile);
    String routeError;
    if (!applyRouteProfilePatch(root, profile, routeError)) {
        logProfileSetError(routeError);
        return;
    }
    applySlotProfilePatch(root, profile, request.id == g_activeProfile);

    bool activeApplied = false;
    if (!persistPatchedProfile(request.id, profile, activeApplied)) {
        logProfileSetError("Profile patch could not be persisted");
        return;
    }
    if (activeApplied && root.containsKey("slots")) {
        markAllFilterTuningRemoteControlActive();
    }

    LOG_PRINTF("{\"type\":\"response\",\"status\":\"ok\",\"command\":\"SET_PROFILE\","
               "\"profile\":%u,\"active_profile\":%u,\"profile_updated\":true,"
               "\"active_applied\":%s}\n",
               static_cast<unsigned>(request.id), static_cast<unsigned>(g_activeProfile),
               activeApplied ? "true" : "false");
}
