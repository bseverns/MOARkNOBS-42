#include "protocol/ProfileMacroHandlers.h"

#include <ArduinoJson.h>

#include "Arpeggiator.h"
#include "ConfigManager.h"
#include "Globals.h"
#include "LFO/LFOManager.h"
#include "Log.h"
#include "Modes.h"
#include "protocol/ProfileCommands.h"
#include "protocol/MidiInputConfigCodec.h"
#include "protocol/SceneStorage.h"

// ProfileMacroHandlers.cpp is the host-facing wrapper around profile slots,
// macro snapshots, and a few arp utility commands.
//
// Reading order:
// 1. JSON response helpers
// 2. profile export (`GET_PROFILE`)
// 3. arp utility commands
// 4. profile save/load/reset command wrappers
// 5. macro snapshot save/recall wrappers

namespace {
constexpr size_t kProfileFullJsonCapacity = 24576;
constexpr size_t kProfileModulationJsonCapacity = 8192;

// Full profile export includes 42 slots with EF state and can exceed RAM1 stack budget.
// Keep the scratch documents in RAM2, matching the heavier config/matrix exports.
DMAMEM StaticJsonDocument<kProfileFullJsonCapacity> profileFullDoc;
DMAMEM StaticJsonDocument<kProfileModulationJsonCapacity> profileModulationDoc;

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
    obj["destination_mode"] = settings.destinationMode;
}

const char *profileSlotTypeName(uint8_t type) {
    switch (static_cast<MIDIMessageType>(type)) {
    case MIDIMessageType::OFF:
        return "OFF";
    case MIDIMessageType::CC:
        return "CC";
    case MIDIMessageType::Note:
        return "NOTE";
    case MIDIMessageType::PitchBend:
        return "PITCH_BEND";
    case MIDIMessageType::ProgramChange:
        return "PROGRAM";
    case MIDIMessageType::Aftertouch:
        return "AFTERTOUCH";
    case MIDIMessageType::ModWheel:
        return "MOD_WHEEL";
    case MIDIMessageType::NRPN:
        return "NRPN";
    case MIDIMessageType::RPN:
        return "RPN";
    case MIDIMessageType::SysEx:
        return "SYSEX";
    default:
        return "OFF";
    }
}

int readOptionalCommandValue(const String &command, int fallback) {
    int comma = command.indexOf(',');
    return comma >= 0 ? command.substring(comma + 1).toInt() : fallback;
}

bool readProfileSlotArgument(const String &command, uint8_t fallback, int &id) {
    id = readOptionalCommandValue(command, static_cast<int>(fallback));
    return id >= 0 && id < NUM_PROFILES;
}

bool isModulationProfileRequest(const String &command) {
    int firstComma = command.indexOf(',');
    if (firstComma < 0) {
        return false;
    }
    int secondComma = command.indexOf(',', firstComma + 1);
    if (secondComma < 0) {
        return false;
    }
    String mode = command.substring(secondComma + 1);
    mode.trim();
    return mode.equalsIgnoreCase("mod") || mode.equalsIgnoreCase("modulation") ||
           mode.equalsIgnoreCase("utilities") || mode.equalsIgnoreCase("lfo") ||
           mode.equalsIgnoreCase("arp");
}

bool readRequiredPotSlotArgument(const String &command, int &slot) {
    int comma = command.indexOf(',');
    if (comma < 0) {
        return false;
    }
    slot = command.substring(comma + 1).toInt();
    return slot >= 0 && slot < NUM_SLOTS;
}

void writeProfileArp(JsonObject arp, const ProfileData &profile) {
    arp["length_ticks"] = profile.arp.lengthTicks;
    arp["shape"] = profile.arp.shape;
    arp["swing_percent"] = profile.arp.swingPercent;
    arp["gate_percent"] = profile.arp.gatePercent;
    arp["octave_range"] = profile.arp.octaveRange;
    arp["pattern_length"] = profile.arp.patternLength;
    JsonArray assignedSlots = arp.createNestedArray("assigned_slots");
    for (uint8_t slot = 0; slot < NUM_SLOTS; ++slot) {
        if ((profile.arp.assignedSlots[slot / 8U] & (1U << (slot % 8U))) != 0) {
            assignedSlots.add(slot);
        }
    }
}

void writeProfileLed(JsonObject led, const ProfileData &profile) {
    led["brightness"] = profile.led.brightness;
    JsonObject rgb = led.createNestedObject("rgb");
    rgb["r"] = profile.led.r;
    rgb["g"] = profile.led.g;
    rgb["b"] = profile.led.b;
}

void writeProfileClock(JsonObject clock, const ProfileData &profile) {
    clock["follow_external"] = profile.clock.followExternalClock != 0;
    clock["clock_out_enabled"] = profile.clock.clockOutEnabled != 0;
    clock["tapped_bpm"] = profile.clock.tappedBpm;
}

void writeProfileNoteDynamics(JsonObject noteDynamics, const ProfileData &profile) {
    noteDynamics["velocity_shift"] = static_cast<int>(profile.noteDynamics.velocityShift);
    noteDynamics["change_probability"] = profile.noteDynamics.changeProbability;
}

void writeProfileJitter(JsonObject jitter, const ProfileData &profile) {
    jitter["depth"] = profile.jitter.depth;
    jitter["smoothness"] = profile.jitter.smoothness;
}

void writeProfileLfos(JsonArray lfos, const ProfileData &profile) {
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
}

void writeProfileRoutes(JsonArray routes, const ProfileData &profile) {
    for (uint8_t i = 0; i < profile.routeCount && i < PROFILE_MAX_ROUTES; ++i) {
        JsonObject route = routes.createNestedObject();
        route["index"] = i;
        route["type"] = profile.routes[i].type;
        route["lfo"] = profile.routes[i].lfoIndex;
        route["depth"] = profile.routes[i].depth;
        route["target"] = profile.routes[i].target;
        if (profile.routes[i].type == static_cast<uint8_t>(LFOManager::Route::Type::SlotValue)) {
            route["slot"] = profile.routes[i].target;
        }
        route["channel"] = profile.routes[i].channel;
        route["cc_msb"] = profile.routes[i].ccMsb;
        route["cc_lsb"] = profile.routes[i].ccLsb;
        route["amount"] = profile.routes[i].amount;
        route["min"] = profile.routes[i].minValue;
        route["max"] = profile.routes[i].maxValue;
    }
}

void writeProfileSlots(JsonArray slots, const ProfileData &profile) {
    for (uint8_t i = 0; i < NUM_SLOTS; ++i) {
        const MIDISlot &runtimeSlot = configManager.getSlot(i);
        JsonObject slot = slots.createNestedObject();
        slot["index"] = i;
        const uint8_t type = static_cast<uint8_t>(runtimeSlot.type);
        slot["type"] = type;
        slot["type_name"] = profileSlotTypeName(type);
        slot["channel"] = profile.slots[i].midiChannel;
        slot["midiChannel"] = profile.slots[i].midiChannel;
        slot["data1"] = runtimeSlot.data1;
        slot["active"] = runtimeSlot.active;
        slot["arp_note"] = runtimeSlot.arpNote;
        slot["arpNote"] = runtimeSlot.arpNote;
        JsonObject ef = slot.createNestedObject("ef");
        writeProfileEf(ef, profile.slots[i].ef);
    }
}

template <size_t Capacity>
void writeProfileResponse(StaticJsonDocument<Capacity> &doc, uint8_t id, bool stored,
                          const ProfileData &profile,
                          const ProfileModulationExtension &modulation, bool includeSlots) {
    doc.clear();
    doc["profile"] = id;
    doc["active_profile"] = g_activeProfile;
    doc["active"] = id == g_activeProfile;
    doc["stored"] = stored;

    JsonObject arp = doc.createNestedObject("arp");
    writeProfileArp(arp, profile);

    JsonObject led = doc.createNestedObject("led");
    writeProfileLed(led, profile);

    JsonObject clock = doc.createNestedObject("clock");
    writeProfileClock(clock, profile);

    JsonObject noteDynamics = doc.createNestedObject("note_dynamics");
    writeProfileNoteDynamics(noteDynamics, profile);

    JsonObject jitter = doc.createNestedObject("jitter");
    writeProfileJitter(jitter, profile);

    JsonArray lfos = doc.createNestedArray("lfos");
    writeProfileLfos(lfos, profile);

    JsonArray routes = doc.createNestedArray("routes");
    writeProfileRoutes(routes, profile);

    JsonArray midiInputBindings = doc.createNestedArray("midiInputBindings");
    MidiInputConfigCodec::write(midiInputBindings, modulation.midiInputBindings,
                                modulation.midiInputBindingCount);

    if (includeSlots) {
        JsonArray slots = doc.createNestedArray("slots");
        writeProfileSlots(slots, profile);
    }
}
} // namespace

// 2. Profile export lane.
void handleGetProfileCommand(const String &command) {
    // If a profile slot has never been persisted, fall back to a live runtime snapshot so hosts
    // still receive a complete payload.
    int id = 0;
    if (!readProfileSlotArgument(command, g_activeProfile, id)) {
        LOG_PRINTLN("{\"type\":\"error\",\"code\":\"invalid_profile\"}");
        return;
    }
    ProfileData profile{};
    bool stored = configManager.loadProfileSettings(static_cast<uint8_t>(id), profile);
    if (!stored) {
        profile = captureProfileSnapshot();
    }

    ProfileModulationExtension modulation{};
    if (!configManager.loadProfileModulation(static_cast<uint8_t>(id), modulation)) {
        modulation = id == g_activeProfile ? captureProfileModulation()
                                           : defaultProfileModulationSnapshot();
    }

    const uint8_t profileId = static_cast<uint8_t>(id);
    if (isModulationProfileRequest(command)) {
        writeProfileResponse(profileModulationDoc, profileId, stored, profile, modulation, false);
        sendJsonResponse(profileModulationDoc);
    } else {
        writeProfileResponse(profileFullDoc, profileId, stored, profile, modulation, true);
        sendJsonResponse(profileFullDoc);
    }
}

// 3. Arp utility commands.
void handleArpStartCommand(const String &command) {
    int slot = -1;
    if (!readRequiredPotSlotArgument(command, slot)) {
        LOG_PRINTLN("{\"type\":\"error\",\"code\":\"missing_slot\"}");
        return;
    }
    const MIDISlot &slotConfig = configManager.getSlot(static_cast<uint8_t>(slot));
    arpeggiator.setBaseNoteSource(Arpeggiator::BaseNoteSource::Slot);
    arpeggiator.start(static_cast<uint8_t>(slot));
    StaticJsonDocument<288> response;
    response["arp_started"] = true;
    response["slot"] = slot;
    response["active"] = arpeggiator.isActive();
    response["slot_active"] = slotConfig.active;
    response["slot_type"] = profileSlotTypeName(static_cast<uint8_t>(slotConfig.type));
    response["channel"] = slotConfig.midiChannel;
    response["data1"] = slotConfig.data1;
    response["arp_note"] = slotConfig.arpNote;
    response["arpNote"] = slotConfig.arpNote;
    sendJsonResponse(response);
}

void handleArpStopCommand(const String &command) {
    int comma = command.indexOf(',');
    if (comma >= 0) {
        int slot = -1;
        if (!readRequiredPotSlotArgument(command, slot)) {
            LOG_PRINTLN("{\"type\":\"error\",\"code\":\"invalid_slot\"}");
            return;
        }
        arpeggiator.stop(static_cast<uint8_t>(slot));
    } else {
        arpeggiator.stop();
    }
    StaticJsonDocument<128> response;
    response["arp_stopped"] = true;
    response["active"] = arpeggiator.isActive();
    sendJsonResponse(response);
}

// 4. Profile lifecycle command wrappers.
void handleLoadProfileCommand(const String &command) {
    int id = 0;
    bool valid = readProfileSlotArgument(command, g_activeProfile, id);
    StaticJsonDocument<160> response;
    response["profile"] = id;
    response["active_profile"] = g_activeProfile;
    if (!valid || !loadProfileSlot(static_cast<uint8_t>(id))) {
        response["profile_loaded"] = false;
        response["error"] = "Profile load failed";
        sendJsonResponse(response);
        return;
    }
    response["profile_loaded"] = true;
    response["active_profile"] = g_activeProfile;
    sendJsonResponse(response);
}

void handleResetProfileCommand(const String &command) {
    int id = 0;
    bool valid = readProfileSlotArgument(command, g_activeProfile, id);
    StaticJsonDocument<160> response;
    response["profile"] = id;
    response["active_profile"] = g_activeProfile;
    if (!valid || !resetProfileSlot(static_cast<uint8_t>(id))) {
        response["profile_reset"] = false;
        response["error"] = "Profile reset failed";
        sendJsonResponse(response);
        return;
    }
    response["profile_reset"] = true;
    response["active_profile"] = g_activeProfile;
    sendJsonResponse(response);
}

void handleSaveProfileCommand(const String &command) {
    int id = 0;
    bool valid = readProfileSlotArgument(command, g_activeProfile, id);
    StaticJsonDocument<160> response;
    response["profile"] = id;
    response["active_profile"] = g_activeProfile;
    if (!valid || !saveCurrentProfileSlot(static_cast<uint8_t>(id))) {
        response["profile_saved"] = false;
        response["error"] = "Profile save failed";
        sendJsonResponse(response);
        return;
    }
    response["profile_saved"] = true;
    response["active_profile"] = g_activeProfile;
    sendJsonResponse(response);
}

// 5. Macro snapshot wrappers.
void handleRecallMacroSlotCommand() {
    SceneStorage::ConfigState snapshot{};
    bool available = SceneStorage::macroSnapshotAvailable();
    bool recalled = false;
    if (available && SceneStorage::loadMacroSnapshot(snapshot)) {
        recalled = SceneStorage::applyConfigState(snapshot, true);
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
