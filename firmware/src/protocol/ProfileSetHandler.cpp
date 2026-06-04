#include "protocol/ProfileSetHandler.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <cctype>

#include "ConfigManager.h"
#include "EnvelopeFollower.h"
#include "Globals.h"
#include "LFO/LFOManager.h"
#include "Log.h"
#include "Modes.h"

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
};

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
    return request.payload.length() > 0;
}

bool parseProfilePayloadDocument(const String &payload, StaticJsonDocument<12288> &doc) {
    DeserializationError err = deserializeJson(doc, payload);
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

void applyRouteProfilePatch(JsonObject root, ProfileData &profile) {
    profile.routeCount = 0;
    if (!root.containsKey("routes")) {
        return;
    }

    JsonArray routes = root["routes"].as<JsonArray>();
    for (JsonObject route : routes) {
        if (profile.routeCount >= PROFILE_MAX_ROUTES) {
            break;
        }
        ProfileLfoRoute &out = profile.routes[profile.routeCount];
        out.type = static_cast<uint8_t>(route["type"].as<int>());
        out.lfoIndex = static_cast<uint8_t>(route["lfo"].as<int>());
        out.depth = route["depth"].as<float>();
        if (out.type == static_cast<uint8_t>(LFOManager::Route::Type::SlotValue) &&
            route.containsKey("slot")) {
            out.target = static_cast<uint8_t>(route["slot"].as<int>());
        } else {
            out.target = static_cast<uint8_t>(route["target"].as<int>());
        }
        out.channel = static_cast<uint8_t>(route["channel"].as<int>());
        out.ccMsb = static_cast<uint8_t>(route["cc_msb"].as<int>());
        out.ccLsb = static_cast<uint8_t>(route["cc_lsb"].as<int>());
        out.amount = route.containsKey("amount") ? static_cast<int8_t>(route["amount"].as<int>())
                                                 : static_cast<int8_t>(100);
        out.minValue = route.containsKey("min") ? static_cast<uint8_t>(route["min"].as<int>())
                                                : static_cast<uint8_t>(0);
        out.maxValue = route.containsKey("max") ? static_cast<uint8_t>(route["max"].as<int>())
                                                : static_cast<uint8_t>(127);
        profile.routeCount++;
    }
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
        if (slot.containsKey("data1") || slot.containsKey("cc")) {
            const uint8_t data1 =
                clampedU8(slot, "data1", "cc", 0, 127, liveSlot ? liveSlot->data1 : 0);
            if (liveSlot) {
                liveSlot->data1 = data1;
                liveSlotChanged = true;
            }
        }
        if (!slot.containsKey("data1") && !slot.containsKey("cc") &&
            (slot.containsKey("arpNote") || slot.containsKey("arp_note"))) {
            const uint8_t note =
                clampedU8(slot, "arpNote", "arp_note", 0, 127, liveSlot ? liveSlot->data1 : 0);
            if (liveSlot) {
                liveSlot->data1 = note;
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
    if (!configManager.saveProfileSettings(id, profile)) {
        return false;
    }

    activeApplied = false;
    if (id != g_activeProfile) {
        return true;
    }

    ProfileData stored{};
    if (configManager.loadProfileSettings(id, stored)) {
        applyProfileSnapshot(stored, true);
        activeApplied = true;
    }
    return true;
}
} // namespace

void handleSetProfilePayloadCommand(const String &command) {
    // Merge incoming JSON onto a captured snapshot so callers may send sparse profile patches
    // instead of a full profile document every time.
    ProfileSetRequest request;
    if (!parseProfileSetRequest(command, request)) {
        LOG_PRINTLN("{\"type\":\"error\",\"code\":\"bad_request\"}");
        return;
    }

    StaticJsonDocument<12288> doc;
    if (!parseProfilePayloadDocument(request.payload, doc)) {
        LOG_PRINTLN("{\"type\":\"error\",\"code\":\"bad_request\"}");
        return;
    }

    ProfileData profile = captureProfileSnapshot();
    JsonObject root = doc.as<JsonObject>();
    applyArpProfilePatch(root, profile);
    applyLedProfilePatch(root, profile);
    applyLfoProfilePatch(root, profile);
    applyRouteProfilePatch(root, profile);
    applySlotProfilePatch(root, profile, request.id == g_activeProfile);

    bool activeApplied = false;
    if (!persistPatchedProfile(request.id, profile, activeApplied)) {
        LOG_PRINTLN("{\"type\":\"error\",\"code\":\"bad_request\"}");
        return;
    }

    LOG_PRINTF("{\"type\":\"response\",\"status\":\"ok\",\"command\":\"SET_PROFILE\","
               "\"profile\":%u,\"profile_updated\":true,\"active_applied\":%s}\n",
               static_cast<unsigned>(request.id), activeApplied ? "true" : "false");
}
