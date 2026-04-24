#include "protocol/ProfileSetHandler.h"

#include <Arduino.h>
#include <ArduinoJson.h>

#include "ConfigManager.h"
#include "EnvelopeFollower.h"
#include "Globals.h"
#include "Log.h"
#include "Modes.h"

namespace {
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
    return true;
}
} // namespace

void handleSetProfilePayloadCommand(const String &command) {
    // Merge incoming JSON onto a captured snapshot so callers may send sparse profile patches
    // instead of a full profile document every time.
    int firstComma = command.indexOf(',');
    int secondComma = command.indexOf(',', firstComma + 1);
    if (firstComma < 0 || secondComma < 0) {
        LOG_PRINTLN("{\"type\":\"error\",\"code\":\"bad_request\"}");
        return;
    }
    uint8_t id = static_cast<uint8_t>(command.substring(firstComma + 1, secondComma).toInt());
    if (id >= NUM_PROFILES) {
        LOG_PRINTLN("{\"type\":\"error\",\"code\":\"bad_request\"}");
        return;
    }
    String payload = command.substring(secondComma + 1);
    payload.trim();
    if (payload.length() == 0) {
        LOG_PRINTLN("{\"type\":\"error\",\"code\":\"bad_request\"}");
        return;
    }
    StaticJsonDocument<12288> doc;
    DeserializationError err = deserializeJson(doc, payload);
    if (err) {
        LOG_PRINTLN("{\"type\":\"error\",\"code\":\"bad_request\"}");
        return;
    }
    ProfileData profile = captureProfileSnapshot();
    JsonObject root = doc.as<JsonObject>();

    if (root.containsKey("arp")) {
        JsonObject arp = root["arp"].as<JsonObject>();
        if (arp.containsKey("length_ticks")) {
            profile.arp.lengthTicks =
                clampedU8(arp, "length_ticks", 1, 255, profile.arp.lengthTicks);
        }
        if (arp.containsKey("shape")) {
            profile.arp.shape = clampedU8(arp, "shape", 0, 255, profile.arp.shape);
        }
        if (arp.containsKey("swing_percent")) {
            profile.arp.swingPercent =
                clampedU8(arp, "swing_percent", 0, 80, profile.arp.swingPercent);
        }
        if (arp.containsKey("gate_percent")) {
            profile.arp.gatePercent =
                clampedU8(arp, "gate_percent", 5, 100, profile.arp.gatePercent);
        }
        if (arp.containsKey("octave_range")) {
            profile.arp.octaveRange = clampedU8(arp, "octave_range", 0, 3, profile.arp.octaveRange);
        }
    }

    if (root.containsKey("led")) {
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

    if (root.containsKey("lfos")) {
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

    profile.routeCount = 0;
    if (root.containsKey("routes")) {
        JsonArray routes = root["routes"].as<JsonArray>();
        for (JsonObject route : routes) {
            if (profile.routeCount >= PROFILE_MAX_ROUTES) {
                break;
            }
            ProfileLfoRoute &out = profile.routes[profile.routeCount];
            out.type = static_cast<uint8_t>(route["type"].as<int>());
            out.lfoIndex = static_cast<uint8_t>(route["lfo"].as<int>());
            out.depth = route["depth"].as<float>();
            out.target = static_cast<uint8_t>(route["target"].as<int>());
            out.channel = static_cast<uint8_t>(route["channel"].as<int>());
            out.ccMsb = static_cast<uint8_t>(route["cc_msb"].as<int>());
            out.ccLsb = static_cast<uint8_t>(route["cc_lsb"].as<int>());
            profile.routeCount++;
        }
    }

    if (root.containsKey("slots")) {
        JsonArray slots = root["slots"].as<JsonArray>();
        for (JsonObject slot : slots) {
            uint8_t index = static_cast<uint8_t>(slot["index"].as<int>());
            if (index >= NUM_SLOTS) {
                continue;
            }
            if (slot.containsKey("channel")) {
                profile.slots[index].midiChannel =
                    clampedU8(slot, "channel", 1, 16, profile.slots[index].midiChannel);
            }
            if (slot.containsKey("ef")) {
                JsonObject ef = slot["ef"].as<JsonObject>();
                parseProfileEf(ef, profile.slots[index].ef);
            }
        }
    }

    if (!configManager.saveProfileSettings(id, profile)) {
        LOG_PRINTLN("{\"type\":\"error\",\"code\":\"bad_request\"}");
        return;
    }
    if (id == g_activeProfile) {
        // Keep runtime state in lockstep when the active profile slot is edited remotely.
        ProfileData stored{};
        if (configManager.loadProfileSettings(id, stored)) {
            applyProfileSnapshot(stored, true);
        }
    }
    LOG_PRINTLN("{\"type\":\"response\",\"status\":\"ok\"}");
}
