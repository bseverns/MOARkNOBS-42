#include "protocol/ProfileSetHandler.h"

#include <Arduino.h>
#include <ArduinoJson.h>

#include "ConfigManager.h"
#include "EnvelopeFollower.h"
#include "Globals.h"
#include "Log.h"
#include "Modes.h"

namespace {
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
        out.attackMs = static_cast<uint16_t>(obj["attack_ms"].as<int>());
    }
    if (obj.containsKey("release_ms")) {
        out.releaseMs = static_cast<uint16_t>(obj["release_ms"].as<int>());
    }
    if (obj.containsKey("rms_ms")) {
        out.rmsWindowMs = static_cast<uint16_t>(obj["rms_ms"].as<int>());
    }
    if (obj.containsKey("baseline_tau_ms")) {
        out.baselineTauMs = static_cast<uint16_t>(obj["baseline_tau_ms"].as<int>());
    }
    if (obj.containsKey("gain_tau_ms")) {
        out.gainTauMs = static_cast<uint16_t>(obj["gain_tau_ms"].as<int>());
    }
    if (obj.containsKey("gate_threshold")) {
        out.gateThreshold = static_cast<uint8_t>(obj["gate_threshold"].as<int>());
    }
    if (obj.containsKey("gate_hysteresis")) {
        out.gateHysteresis = static_cast<uint8_t>(obj["gate_hysteresis"].as<int>());
    }
    if (obj.containsKey("activity_threshold")) {
        out.activityThreshold = static_cast<uint8_t>(obj["activity_threshold"].as<int>());
    }
    if (obj.containsKey("gain_target")) {
        out.gainTarget = static_cast<uint8_t>(obj["gain_target"].as<int>());
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
            profile.arp.lengthTicks = static_cast<uint8_t>(arp["length_ticks"].as<int>());
        }
        if (arp.containsKey("shape")) {
            profile.arp.shape = static_cast<uint8_t>(arp["shape"].as<int>());
        }
        if (arp.containsKey("swing_percent")) {
            profile.arp.swingPercent = static_cast<uint8_t>(arp["swing_percent"].as<int>());
        }
        if (arp.containsKey("gate_percent")) {
            profile.arp.gatePercent = static_cast<uint8_t>(arp["gate_percent"].as<int>());
        }
        if (arp.containsKey("octave_range")) {
            profile.arp.octaveRange = static_cast<uint8_t>(arp["octave_range"].as<int>());
        }
    }

    if (root.containsKey("led")) {
        JsonObject led = root["led"].as<JsonObject>();
        if (led.containsKey("brightness")) {
            profile.led.brightness = static_cast<uint8_t>(led["brightness"].as<int>());
        }
        if (led.containsKey("rgb")) {
            JsonObject rgb = led["rgb"].as<JsonObject>();
            profile.led.r = static_cast<uint8_t>(rgb["r"].as<int>());
            profile.led.g = static_cast<uint8_t>(rgb["g"].as<int>());
            profile.led.b = static_cast<uint8_t>(rgb["b"].as<int>());
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
                profile.slots[index].midiChannel = static_cast<uint8_t>(slot["channel"].as<int>());
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
