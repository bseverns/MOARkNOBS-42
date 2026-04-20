#include "protocol/ProfileMacroHandlers.h"

#include <ArduinoJson.h>

#include "ConfigManager.h"
#include "Globals.h"
#include "Log.h"
#include "Modes.h"
#include "protocol/ProfileCommands.h"
#include "protocol/SceneStorage.h"

namespace {
template <size_t Capacity> void sendJsonResponse(const StaticJsonDocument<Capacity> &doc) {
    if (doc.overflowed()) {
        LOG_PRINTLN("{\"type\":\"error\",\"code\":\"json_overflow\"}");
        return;
    }
    String payload;
    serializeJson(doc, payload);
    LOG_PRINTLN(payload);
}

void writeProfileEf(JsonObject obj, const ProfileEfSettings &settings) {
    obj["mode"] = settings.mode;
    obj["auto_baseline"] = settings.autoBaseline != 0;
    obj["auto_gain"] = settings.autoGain != 0;
    obj["attack_ms"] = settings.attackMs;
    obj["release_ms"] = settings.releaseMs;
    obj["rms_ms"] = settings.rmsWindowMs;
    obj["baseline_tau_ms"] = settings.baselineTauMs;
    obj["gain_tau_ms"] = settings.gainTauMs;
    obj["gate_threshold"] = settings.gateThreshold;
    obj["gate_hysteresis"] = settings.gateHysteresis;
    obj["activity_threshold"] = settings.activityThreshold;
    obj["gain_target"] = settings.gainTarget;
}
} // namespace

void handleGetProfileCommand(const String &command) {
    // If a profile slot has never been persisted, fall back to a live runtime snapshot so hosts
    // still receive a complete payload.
    int comma = command.indexOf(',');
    uint8_t id = g_activeProfile;
    if (comma >= 0) {
        id = static_cast<uint8_t>(command.substring(comma + 1).toInt());
    }
    if (id >= NUM_PROFILES) {
        LOG_PRINTLN("{\"type\":\"error\",\"code\":\"invalid_profile\"}");
        return;
    }
    ProfileData profile{};
    bool stored = configManager.loadProfileSettings(id, profile);
    if (!stored) {
        profile = captureProfileSnapshot();
    }

    StaticJsonDocument<12288> doc;
    doc["profile"] = id;
    doc["stored"] = stored;

    JsonObject arp = doc.createNestedObject("arp");
    arp["length_ticks"] = profile.arp.lengthTicks;
    arp["shape"] = profile.arp.shape;
    arp["swing_percent"] = profile.arp.swingPercent;
    arp["gate_percent"] = profile.arp.gatePercent;
    arp["octave_range"] = profile.arp.octaveRange;

    JsonObject led = doc.createNestedObject("led");
    led["brightness"] = profile.led.brightness;
    JsonObject rgb = led.createNestedObject("rgb");
    rgb["r"] = profile.led.r;
    rgb["g"] = profile.led.g;
    rgb["b"] = profile.led.b;

    JsonArray lfos = doc.createNestedArray("lfos");
    for (uint8_t i = 0; i < PROFILE_LFO_COUNT; ++i) {
        JsonObject lfo = lfos.createNestedObject();
        lfo["index"] = i;
        lfo["shape"] = profile.lfos[i].shape;
        lfo["frequency_hz"] = profile.lfos[i].frequencyHz;
        lfo["depth"] = profile.lfos[i].depth;
        lfo["bipolar"] = profile.lfos[i].bipolar != 0;
        lfo["sync"] = profile.lfos[i].syncEnabled != 0;
        lfo["sync_ratio"] = profile.lfos[i].syncRatio;
    }

    JsonArray routes = doc.createNestedArray("routes");
    for (uint8_t i = 0; i < profile.routeCount && i < PROFILE_MAX_ROUTES; ++i) {
        JsonObject route = routes.createNestedObject();
        route["index"] = i;
        route["type"] = profile.routes[i].type;
        route["lfo"] = profile.routes[i].lfoIndex;
        route["depth"] = profile.routes[i].depth;
        route["target"] = profile.routes[i].target;
        route["channel"] = profile.routes[i].channel;
        route["cc_msb"] = profile.routes[i].ccMsb;
        route["cc_lsb"] = profile.routes[i].ccLsb;
    }

    JsonArray slots = doc.createNestedArray("slots");
    for (uint8_t i = 0; i < NUM_SLOTS; ++i) {
        JsonObject slot = slots.createNestedObject();
        slot["index"] = i;
        slot["channel"] = profile.slots[i].midiChannel;
        JsonObject ef = slot.createNestedObject("ef");
        writeProfileEf(ef, profile.slots[i].ef);
    }

    if (doc.overflowed()) {
        LOG_PRINTLN("{\"type\":\"error\",\"code\":\"json_overflow\"}");
        return;
    }
    String payload;
    serializeJson(doc, payload);
    LOG_PRINTLN(payload);
}

void handleLoadProfileCommand(const String &command) {
    int comma = command.indexOf(',');
    int id = comma >= 0 ? command.substring(comma + 1).toInt() : static_cast<int>(g_activeProfile);
    StaticJsonDocument<160> response;
    response["profile"] = id;
    if (id < 0 || id >= NUM_PROFILES || !loadProfileSlot(static_cast<uint8_t>(id))) {
        response["profile_loaded"] = false;
        response["error"] = "Profile load failed";
        sendJsonResponse(response);
        return;
    }
    response["profile_loaded"] = true;
    sendJsonResponse(response);
}

void handleRecallMacroSlotCommand() {
    SceneStorage::ConfigState snapshot{};
    bool available = SceneStorage::macroSnapshotAvailable();
    bool recalled = false;
    if (available && SceneStorage::loadMacroSnapshot(snapshot)) {
        SceneStorage::applyConfigState(snapshot, true);
        recalled = true;
    }
    StaticJsonDocument<128> response;
    response["macro_recalled"] = recalled;
    response["macro_available"] = SceneStorage::macroSnapshotAvailable();
    if (!recalled) {
        response["error"] = available ? "Macro recall failed" : "No macro stored";
    }
    sendJsonResponse(response);
}

void handleSaveMacroSlotCommand() {
    SceneStorage::ConfigState snapshot = SceneStorage::captureConfigState();
    bool saved = SceneStorage::saveMacroSnapshot(snapshot);
    StaticJsonDocument<128> response;
    response["macro_saved"] = saved;
    response["macro_available"] = SceneStorage::macroSnapshotAvailable();
    if (!saved) {
        response["error"] = "Macro snapshot save failed";
    }
    sendJsonResponse(response);
}

void handleResetProfileCommand(const String &command) {
    int comma = command.indexOf(',');
    int id = comma >= 0 ? command.substring(comma + 1).toInt() : static_cast<int>(g_activeProfile);
    StaticJsonDocument<160> response;
    response["profile"] = id;
    if (id < 0 || id >= NUM_PROFILES || !resetProfileSlot(static_cast<uint8_t>(id))) {
        response["profile_reset"] = false;
        response["error"] = "Profile reset failed";
        sendJsonResponse(response);
        return;
    }
    response["profile_reset"] = true;
    sendJsonResponse(response);
}

void handleSaveProfileCommand(const String &command) {
    int comma = command.indexOf(',');
    int id = comma >= 0 ? command.substring(comma + 1).toInt() : static_cast<int>(g_activeProfile);
    StaticJsonDocument<160> response;
    response["profile"] = id;
    if (id < 0 || id >= NUM_PROFILES || !saveCurrentProfileSlot(static_cast<uint8_t>(id))) {
        response["profile_saved"] = false;
        response["error"] = "Profile save failed";
        sendJsonResponse(response);
        return;
    }
    response["profile_saved"] = true;
    sendJsonResponse(response);
}
