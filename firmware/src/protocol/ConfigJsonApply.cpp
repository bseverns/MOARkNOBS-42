#include "protocol/ConfigJsonApply.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <array>
#include <cmath>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "ARGMixer.h"
#include "BoardPowerProfile.h"
#include "ConfigManager.h"
#include "DiagnosticRecord.h"
#include "EfSettingsUtils.h"
#include "EnvelopeFollower.h"
#include "FirmwareState.h"
#include "Globals.h"
#include "Log.h"
#include "Modes.h"
#include "Protocol.h"
#include "UI.h"
#include "Utility.h"
#include "protocol/ProtocolErrors.h"
#include "protocol/SysExTemplateCodec.h"

namespace {
Utility::BulkConfigAssembler bulkConfigAssembler;
uint32_t lastAckSequence = 0;
String lastAckChecksum;
String lastAppliedChecksum;
// Bulk SET_ALL parse state is large by design; keep the reusable document in
// RAM2 so unit-test and runtime stacks keep more RAM1 headroom.
DMAMEM StaticJsonDocument<Utility::kMaxBulkConfigSize> bulkApplyDoc;

struct BulkApplyIdentity {
    uint32_t sequence = 0;
    String configId;
};

// Device-owned digest of the normalized, applied state.  The host checksum is
// retained as a correlation token, but must never be represented as proof of
// what the firmware actually accepted.
String appliedStateChecksum() {
    const String canonical = configManager.serializeAll();
    uint32_t hash = 2166136261UL; // FNV-1a, stable and small enough for Teensy.
    for (size_t i = 0; i < canonical.length(); ++i) {
        hash ^= static_cast<uint8_t>(canonical[i]);
        hash *= 16777619UL;
    }
    char hex[9] = {0};
    snprintf(hex, sizeof(hex), "%08lx", static_cast<unsigned long>(hash));
    return String(hex);
}

int readIntField(JsonObject obj, const char *primary, const char *alternate, int fallback) {
    if (!obj.isNull() && obj.containsKey(primary)) {
        return obj[primary].as<int>();
    }
    if (!obj.isNull() && obj.containsKey(alternate)) {
        return obj[alternate].as<int>();
    }
    return fallback;
}

float readFloatField(JsonObject obj, const char *primary, const char *alternate, float fallback) {
    if (!obj.isNull() && obj.containsKey(primary)) {
        return obj[primary].as<float>();
    }
    if (!obj.isNull() && obj.containsKey(alternate)) {
        return obj[alternate].as<float>();
    }
    return fallback;
}

uint8_t readClampedU8(JsonObject obj, const char *primary, const char *alternate, int minValue,
                      int maxValue, uint8_t fallback) {
    int value = readIntField(obj, primary, alternate, fallback);
    return static_cast<uint8_t>(constrain(value, minValue, maxValue));
}

uint16_t readClampedU16(JsonObject obj, const char *primary, const char *alternate, int minValue,
                        int maxValue, uint16_t fallback) {
    int value = readIntField(obj, primary, alternate, fallback);
    return static_cast<uint16_t>(constrain(value, minValue, maxValue));
}

bool equalsIgnoreCase(const char *lhs, const char *rhs) {
    if (!lhs || !rhs)
        return false;
    while (*lhs && *rhs) {
        if (tolower(static_cast<unsigned char>(*lhs)) != tolower(static_cast<unsigned char>(*rhs)))
            return false;
        ++lhs;
        ++rhs;
    }
    return *lhs == '\0' && *rhs == '\0';
}

bool parseMIDIType(const char *label, MIDIMessageType &type) {
    if (!label)
        return false;
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
        {"SysEx", "SYSEX", "SYS_EX", MIDIMessageType::SysEx}};
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

EnvelopeFollower::FilterType parseFilterType(const char *label,
                                             EnvelopeFollower::FilterType fallback) {
    if (!label)
        return fallback;
    struct Entry {
        const char *name;
        EnvelopeFollower::FilterType value;
    };
    static constexpr Entry kMap[] = {
        {"LINEAR", EnvelopeFollower::LINEAR},
        {"OPPOSITE_LINEAR", EnvelopeFollower::OPPOSITE_LINEAR},
        {"EXPONENTIAL", EnvelopeFollower::EXPONENTIAL},
        {"RANDOM", EnvelopeFollower::RANDOM},
        {"LOWPASS", EnvelopeFollower::LOWPASS},
        {"HIGHPASS", EnvelopeFollower::HIGHPASS},
        {"BANDPASS", EnvelopeFollower::BANDPASS},
    };
    for (const auto &entry : kMap) {
        if (strcmp(label, entry.name) == 0) {
            return entry.value;
        }
    }
    return fallback;
}

EnvelopeFollower::EFMode parseEfMode(const char *label, EnvelopeFollower::EFMode fallback) {
    if (!label) {
        return fallback;
    }
    struct Entry {
        const char *name;
        EnvelopeFollower::EFMode value;
    };
    static constexpr Entry kMap[] = {
        {"PEAK", EnvelopeFollower::EFMode::Peak},
        {"RMS", EnvelopeFollower::EFMode::RMS},
        {"GATE", EnvelopeFollower::EFMode::Gate},
        {"FOLLOWER", EnvelopeFollower::EFMode::Follower},
    };
    for (const auto &entry : kMap) {
        if (strcmp(label, entry.name) == 0) {
            return entry.value;
        }
    }
    return fallback;
}

EfDestinationMode parseEfDestinationMode(const char *label, EfDestinationMode fallback) {
    if (!label) {
        return fallback;
    }
    struct Entry {
        const char *name;
        EfDestinationMode value;
    };
    static constexpr Entry kMap[] = {
        {"ADD", EfDestinationMode::AddClamp},      {"ADD_CLAMP", EfDestinationMode::AddClamp},
        {"SUBTRACT", EfDestinationMode::Subtract}, {"REPLACE", EfDestinationMode::Replace},
        {"SCALE", EfDestinationMode::Scale},       {"CENTERED", EfDestinationMode::Centered},
    };
    for (const auto &entry : kMap) {
        if (equalsIgnoreCase(label, entry.name)) {
            return entry.value;
        }
    }
    return fallback;
}

void parseEfSettings(JsonObject obj, const EfSettings &defaults, EfSettings &out) {
    out = defaults;
    if (obj.isNull()) {
        return;
    }

    if (obj.containsKey("filter_index")) {
        int idx = constrain(obj["filter_index"].as<int>(), 0, 6);
        out.filterType = static_cast<EfSettings::FilterType>(idx);
    } else if (obj.containsKey("filter")) {
        const char *label = obj["filter"].as<const char *>();
        out.filterType = encodeFilterType(parseFilterType(label, decodeFilterType(out.filterType)));
    } else if (obj.containsKey("filter_name")) {
        const char *label = obj["filter_name"].as<const char *>();
        out.filterType = encodeFilterType(parseFilterType(label, decodeFilterType(out.filterType)));
    }

    if (obj.containsKey("frequency")) {
        out.frequency = obj["frequency"].as<float>();
    } else if (obj.containsKey("freq")) {
        out.frequency = obj["freq"].as<float>();
    }

    if (obj.containsKey("q")) {
        out.q = obj["q"].as<float>();
    }

    if (obj.containsKey("oversample")) {
        out.oversample =
            readClampedU8(obj, "oversample", "oversample", static_cast<int>(EF_OVERSAMPLE_MIN),
                          static_cast<int>(EF_OVERSAMPLE_MAX), out.oversample);
    }

    if (obj.containsKey("smoothing")) {
        out.smoothing = obj["smoothing"].as<float>();
    }

    if (obj.containsKey("baseline")) {
        out.baseline = obj["baseline"].as<float>();
    }

    if (obj.containsKey("gain")) {
        out.gain = obj["gain"].as<float>();
    }

    if (obj.containsKey("destination_mode")) {
        if (obj["destination_mode"].is<const char *>()) {
            out.destinationMode = static_cast<uint8_t>(
                parseEfDestinationMode(obj["destination_mode"].as<const char *>(),
                                       static_cast<EfDestinationMode>(out.destinationMode)));
        } else {
            out.destinationMode =
                readClampedU8(obj, "destination_mode", "destinationMode", 0,
                              static_cast<int>(EfDestinationMode::Centered), out.destinationMode);
        }
    } else if (obj.containsKey("destinationMode")) {
        if (obj["destinationMode"].is<const char *>()) {
            out.destinationMode = static_cast<uint8_t>(
                parseEfDestinationMode(obj["destinationMode"].as<const char *>(),
                                       static_cast<EfDestinationMode>(out.destinationMode)));
        } else {
            out.destinationMode =
                readClampedU8(obj, "destination_mode", "destinationMode", 0,
                              static_cast<int>(EfDestinationMode::Centered), out.destinationMode);
        }
    } else if (obj.containsKey("destination_mode_name")) {
        out.destinationMode = static_cast<uint8_t>(
            parseEfDestinationMode(obj["destination_mode_name"].as<const char *>(),
                                   static_cast<EfDestinationMode>(out.destinationMode)));
    }

    if (obj.containsKey("mode")) {
        if (obj["mode"].is<const char *>()) {
            out.efMode = static_cast<uint8_t>(parseEfMode(
                obj["mode"].as<const char *>(), static_cast<EnvelopeFollower::EFMode>(out.efMode)));
        } else {
            int raw = obj["mode"].as<int>();
            out.efMode = static_cast<uint8_t>(
                constrain(raw, 0, static_cast<int>(EnvelopeFollower::EFMode::Follower)));
        }
    }

    if (obj.containsKey("auto_baseline")) {
        out.autoBaseline = obj["auto_baseline"].as<bool>() ? 1 : 0;
    } else if (obj.containsKey("autoBaseline")) {
        out.autoBaseline = obj["autoBaseline"].as<bool>() ? 1 : 0;
    }

    if (obj.containsKey("auto_gain")) {
        out.autoGain = obj["auto_gain"].as<bool>() ? 1 : 0;
    } else if (obj.containsKey("autoGain")) {
        out.autoGain = obj["autoGain"].as<bool>() ? 1 : 0;
    }

    if (obj.containsKey("attack_ms")) {
        out.attackMs =
            readClampedU16(obj, "attack_ms", "attackMs", static_cast<int>(EF_TIME_MIN_MS),
                           static_cast<int>(EF_TIME_MAX_MS), out.attackMs);
    } else if (obj.containsKey("attackMs")) {
        out.attackMs =
            readClampedU16(obj, "attack_ms", "attackMs", static_cast<int>(EF_TIME_MIN_MS),
                           static_cast<int>(EF_TIME_MAX_MS), out.attackMs);
    }

    if (obj.containsKey("release_ms")) {
        out.releaseMs =
            readClampedU16(obj, "release_ms", "releaseMs", static_cast<int>(EF_TIME_MIN_MS),
                           static_cast<int>(EF_TIME_MAX_MS), out.releaseMs);
    } else if (obj.containsKey("releaseMs")) {
        out.releaseMs =
            readClampedU16(obj, "release_ms", "releaseMs", static_cast<int>(EF_TIME_MIN_MS),
                           static_cast<int>(EF_TIME_MAX_MS), out.releaseMs);
    }

    if (obj.containsKey("rms_ms")) {
        out.rmsWindowMs =
            readClampedU16(obj, "rms_ms", "rmsWindowMs", static_cast<int>(EF_TIME_MIN_MS),
                           static_cast<int>(EF_TIME_MAX_MS), out.rmsWindowMs);
    } else if (obj.containsKey("rmsWindowMs")) {
        out.rmsWindowMs =
            readClampedU16(obj, "rms_ms", "rmsWindowMs", static_cast<int>(EF_TIME_MIN_MS),
                           static_cast<int>(EF_TIME_MAX_MS), out.rmsWindowMs);
    }

    if (obj.containsKey("baseline_tau_ms")) {
        out.baselineTauMs = readClampedU16(obj, "baseline_tau_ms", "baselineTauMs",
                                           static_cast<int>(EF_TIME_MIN_MS),
                                           static_cast<int>(EF_TIME_MAX_MS), out.baselineTauMs);
    } else if (obj.containsKey("baselineTauMs")) {
        out.baselineTauMs = readClampedU16(obj, "baseline_tau_ms", "baselineTauMs",
                                           static_cast<int>(EF_TIME_MIN_MS),
                                           static_cast<int>(EF_TIME_MAX_MS), out.baselineTauMs);
    }

    if (obj.containsKey("gain_tau_ms")) {
        out.gainTauMs =
            readClampedU16(obj, "gain_tau_ms", "gainTauMs", static_cast<int>(EF_TIME_MIN_MS),
                           static_cast<int>(EF_TIME_MAX_MS), out.gainTauMs);
    } else if (obj.containsKey("gainTauMs")) {
        out.gainTauMs =
            readClampedU16(obj, "gain_tau_ms", "gainTauMs", static_cast<int>(EF_TIME_MIN_MS),
                           static_cast<int>(EF_TIME_MAX_MS), out.gainTauMs);
    }

    if (obj.containsKey("gate_threshold")) {
        out.gateThreshold =
            readClampedU8(obj, "gate_threshold", "gateThreshold", 0, 127, out.gateThreshold);
    } else if (obj.containsKey("gateThreshold")) {
        out.gateThreshold =
            readClampedU8(obj, "gate_threshold", "gateThreshold", 0, 127, out.gateThreshold);
    }

    if (obj.containsKey("gate_hysteresis")) {
        out.gateHysteresis =
            readClampedU8(obj, "gate_hysteresis", "gateHysteresis", 0, 127, out.gateHysteresis);
    } else if (obj.containsKey("gateHysteresis")) {
        out.gateHysteresis =
            readClampedU8(obj, "gate_hysteresis", "gateHysteresis", 0, 127, out.gateHysteresis);
    }

    if (obj.containsKey("activity_threshold")) {
        out.activityThreshold = readClampedU8(obj, "activity_threshold", "activityThreshold", 0,
                                              127, out.activityThreshold);
    } else if (obj.containsKey("activityThreshold")) {
        out.activityThreshold = readClampedU8(obj, "activity_threshold", "activityThreshold", 0,
                                              127, out.activityThreshold);
    }

    if (obj.containsKey("gain_target")) {
        out.gainTarget = readClampedU8(obj, "gain_target", "gainTarget", 0, 127, out.gainTarget);
    } else if (obj.containsKey("gainTarget")) {
        out.gainTarget = readClampedU8(obj, "gain_target", "gainTarget", 0, 127, out.gainTarget);
    }

    if (obj.containsKey("index")) {
        int rawIndex = obj["index"].as<int>();
        rawIndex = constrain(rawIndex, -1, static_cast<int>(NUM_ENVELOPES) - 1);
        out.followerIndex = static_cast<int8_t>(rawIndex);
    }
}

ARGMethod parseArgMethod(const char *label, ARGMethod fallback) {
    if (!label)
        return fallback;
    struct Entry {
        const char *name;
        ARGMethod value;
    };
    static constexpr Entry kMap[] = {
        {"PLUS", ARGMethod::PLUS}, {"MIN", ARGMethod::MIN},   {"PECK", ARGMethod::PECK},
        {"SHAV", ARGMethod::SHAV}, {"SQAR", ARGMethod::SQAR}, {"BABS", ARGMethod::BABS},
        {"TABS", ARGMethod::TABS}, {"MULT", ARGMethod::MULT}, {"DIVI", ARGMethod::DIVI},
        {"AVG", ARGMethod::AVG},   {"XABS", ARGMethod::XABS}, {"MAXX", ARGMethod::MAXX},
        {"MINN", ARGMethod::MINN}, {"XORR", ARGMethod::XORR}};
    for (const auto &entry : kMap) {
        if (strcmp(label, entry.name) == 0) {
            return entry.value;
        }
    }
    return fallback;
}

EnvelopeFollower::ARG_Method toFollowerArgMethod(ARGMethod method) {
    return static_cast<EnvelopeFollower::ARG_Method>(static_cast<uint8_t>(method));
}

bool parseHexColor(const char *hex, CRGB &color) {
    if (!hex)
        return false;
    if (hex[0] == '#') {
        ++hex;
    }
    if (strlen(hex) != 6)
        return false;
    char *end = nullptr;
    long value = strtol(hex, &end, 16);
    if (!end || *end != '\0')
        return false;
    color.r = static_cast<uint8_t>((value >> 16) & 0xFF);
    color.g = static_cast<uint8_t>((value >> 8) & 0xFF);
    color.b = static_cast<uint8_t>(value & 0xFF);
    return true;
}

bool parseSlotsArray(JsonObject config, uint32_t seq, JsonArray &slotsJson) {
    if (config.isNull()) {
        emitBulkError("config_missing", "config object absent", seq);
        return false;
    }
    if (!config.containsKey("slots") || !config["slots"].is<JsonArray>()) {
        emitBulkError("slots_missing", "config.slots missing", seq);
        return false;
    }
    slotsJson = config["slots"].as<JsonArray>();
    if (slotsJson.size() != NUM_SLOTS) {
        emitBulkError("slots_size", "unexpected slot count", seq);
        return false;
    }
    return true;
}

EfSettings readDefaultEfSettingsFromConfig(JsonObject config) {
    EfSettings defaultEfSettings{};
    defaultEfSettings.filterType = encodeFilterType(EnvelopeFollower::LINEAR);
    defaultEfSettings.frequency = 1000.0f;
    defaultEfSettings.q = 0.707f;

    if (config.containsKey("filter") && config["filter"].is<JsonObject>()) {
        JsonObject filterObj = config["filter"].as<JsonObject>();
        parseEfSettings(filterObj, defaultEfSettings, defaultEfSettings);
        if (filterObj.containsKey("idle_floor")) {
            configManager.setEfIdleFloor(readClampedU8(filterObj, "idle_floor", "idleFloor", 0, 127,
                                                       configManager.getEfIdleFloor()));
        } else if (filterObj.containsKey("idleFloor")) {
            configManager.setEfIdleFloor(readClampedU8(filterObj, "idle_floor", "idleFloor", 0, 127,
                                                       configManager.getEfIdleFloor()));
        }
    }
    if (config.containsKey("efIdleFloor") || config.containsKey("ef_idle_floor")) {
        configManager.setEfIdleFloor(readClampedU8(config, "ef_idle_floor", "efIdleFloor", 0, 127,
                                                   configManager.getEfIdleFloor()));
    }
    if (config.containsKey("envelopes") && config["envelopes"].is<JsonObject>()) {
        JsonObject envObj = config["envelopes"].as<JsonObject>();
        if (envObj.containsKey("idle_floor") || envObj.containsKey("idleFloor")) {
            configManager.setEfIdleFloor(readClampedU8(envObj, "idle_floor", "idleFloor", 0, 127,
                                                       configManager.getEfIdleFloor()));
        }
    }
    return defaultEfSettings;
}

SlotARGConfig readDefaultArgConfigFromConfig(JsonObject config) {
    SlotARGConfig defaultArg{};
    defaultArg.enabled = configManager.getARGEnable();
    defaultArg.method = static_cast<ARGMethod>(configManager.getARGMethod());
    int storedA = static_cast<int>(configManager.getEnvelopeA());
    if (storedA >= NUM_ENVELOPES) {
        int converted = envelopeIndexFromAnalogPin(storedA);
        storedA = (converted >= 0) ? converted : constrain(storedA, 0, NUM_ENVELOPES - 1);
    }
    int storedB = static_cast<int>(configManager.getEnvelopeB());
    if (storedB >= NUM_ENVELOPES) {
        int converted = envelopeIndexFromAnalogPin(storedB);
        if (converted >= 0) {
            storedB = converted;
        } else {
            storedB = constrain(storedB, 0, NUM_ENVELOPES - 1);
        }
    }
    defaultArg.sourceA = static_cast<uint8_t>(storedA);
    defaultArg.sourceB = static_cast<uint8_t>(storedB);
    defaultArg = sanitizeSlotArg(defaultArg);

    if (config.containsKey("arg") && config["arg"].is<JsonObject>()) {
        JsonObject argObj = config["arg"].as<JsonObject>();
        SlotARGConfig incoming = defaultArg;
        if (argObj.containsKey("enable")) {
            incoming.enabled = argObj["enable"].as<bool>() ? 1 : 0;
        } else if (argObj.containsKey("enabled")) {
            incoming.enabled = argObj["enabled"].as<bool>() ? 1 : 0;
        }
        if (argObj.containsKey("method")) {
            if (argObj["method"].is<const char *>()) {
                ARGMethod parsed =
                    parseArgMethod(argObj["method"].as<const char *>(), incoming.method);
                incoming.method = parsed;
            } else {
                int raw = argObj["method"].as<int>();
                raw = constrain(raw, 0, static_cast<int>(ARGMethod::XORR));
                incoming.method = static_cast<ARGMethod>(raw);
            }
        }
        if (argObj.containsKey("a")) {
            incoming.sourceA =
                static_cast<uint8_t>(constrain(argObj["a"].as<int>(), 0, NUM_ENVELOPES - 1));
        } else if (argObj.containsKey("sourceA")) {
            incoming.sourceA =
                static_cast<uint8_t>(constrain(argObj["sourceA"].as<int>(), 0, NUM_ENVELOPES - 1));
        }
        if (argObj.containsKey("b")) {
            incoming.sourceB =
                static_cast<uint8_t>(constrain(argObj["b"].as<int>(), 0, NUM_ENVELOPES - 1));
        } else if (argObj.containsKey("sourceB")) {
            incoming.sourceB =
                static_cast<uint8_t>(constrain(argObj["sourceB"].as<int>(), 0, NUM_ENVELOPES - 1));
        }

        defaultArg = sanitizeSlotArg(incoming);
    }
    return defaultArg;
}

MIDISlot::EfSettings readSlotEfSettings(JsonObject slotObj, const MIDISlot &slot) {
    MIDISlot::EfSettings settings = slot.efSettings;
    settings.followerIndex = slot.getEnvelopeFollowerIndex();

    int rawEfIndex = settings.followerIndex;
    if (slotObj.containsKey("efIndex")) {
        rawEfIndex = slotObj["efIndex"].as<int>();
    } else if (slotObj.containsKey("ef_index")) {
        rawEfIndex = slotObj["ef_index"].as<int>();
    }

    JsonObject efObj =
        slotObj["ef"].is<JsonObject>() ? slotObj["ef"].as<JsonObject>() : JsonObject();
    if (!efObj.isNull()) {
        parseEfSettings(efObj, settings, settings);
        rawEfIndex = settings.followerIndex;
    }

    if (rawEfIndex >= 0 && rawEfIndex < static_cast<int>(envelopeFollowers.size())) {
        settings.followerIndex = static_cast<int8_t>(rawEfIndex);
    } else {
        settings.followerIndex = -1;
    }
    return settings;
}

SlotARGConfig parseSlotArgConfig(JsonObject slotArgObj, const SlotARGConfig &fallback,
                                 bool allowAnalogPinRouting) {
    SlotARGConfig slotArgConfig = fallback;
    if (slotArgObj.isNull()) {
        return slotArgConfig;
    }

    if (slotArgObj.containsKey("enable")) {
        slotArgConfig.enabled = slotArgObj["enable"].as<bool>() ? 1 : 0;
    } else if (slotArgObj.containsKey("enabled")) {
        slotArgConfig.enabled = slotArgObj["enabled"].as<bool>() ? 1 : 0;
    }

    if (slotArgObj.containsKey("method")) {
        if (slotArgObj["method"].is<const char *>()) {
            ARGMethod parsed =
                parseArgMethod(slotArgObj["method"].as<const char *>(), slotArgConfig.method);
            slotArgConfig.method = parsed;
        } else {
            int raw = slotArgObj["method"].as<int>();
            raw = constrain(raw, 0, static_cast<int>(ARGMethod::XORR));
            slotArgConfig.method = static_cast<ARGMethod>(raw);
        }
    }

    auto parseSource = [&](const char *primary, const char *alternate, uint8_t current) {
        if (!(slotArgObj.containsKey(primary) || slotArgObj.containsKey(alternate))) {
            return current;
        }
        int raw = readIntField(slotArgObj, primary, alternate, current);
        if (allowAnalogPinRouting) {
            int mapped = envelopeIndexFromAnalogPin(raw);
            if (mapped >= 0) {
                raw = mapped;
            }
        }
        return static_cast<uint8_t>(constrain(raw, 0, NUM_ENVELOPES - 1));
    };

    slotArgConfig.sourceA = parseSource("a", "sourceA", slotArgConfig.sourceA);
    slotArgConfig.sourceB = parseSource("b", "sourceB", slotArgConfig.sourceB);
    return sanitizeSlotArg(slotArgConfig);
}

bool applySlotSysExTemplate(JsonObject slotObj, MIDISlot &slot, uint32_t seq) {
    if (slot.type == MIDIMessageType::SysEx) {
        String templateError;
        if (!parseSysExTemplateField(slotObj["sysexTemplate"], slot, templateError)) {
            emitBulkError("sysex_template", templateError.c_str(), seq);
            return false;
        }
        return true;
    }

    clearSysExTemplate(slot);
    return true;
}

void applySlotEnvelopePayload(JsonObject slotObj, uint8_t slotIndex,
                              bool &anySlotPayloadSpecified) {
    if (!(slotObj.containsKey("ef_payload") && slotObj["ef_payload"].is<JsonObject>())) {
        return;
    }

    JsonObject efPayload = slotObj["ef_payload"].as<JsonObject>();
    SlotEnvelopePayload payload = configManager.getSlotEnvelopePayload(slotIndex);

    if (efPayload.containsKey("type_index")) {
        payload.filterType = static_cast<uint8_t>(efPayload["type_index"].as<int>());
    } else if (efPayload.containsKey("type")) {
        const char *label = efPayload["type"].as<const char *>();
        EnvelopeFollower::FilterType mapped =
            parseFilterType(label, static_cast<EnvelopeFollower::FilterType>(payload.filterType));
        payload.filterType = static_cast<uint8_t>(mapped);
    }
    if (efPayload.containsKey("freq")) {
        payload.frequency = efPayload["freq"].as<float>();
    }
    if (efPayload.containsKey("q")) {
        payload.q = efPayload["q"].as<float>();
    }

    configManager.setSlotEnvelopePayload(slotIndex, payload);
    anySlotPayloadSpecified = true;
}

void persistSlotPotRouting(uint8_t slotIndex, uint8_t midiChannel, uint8_t data1) {
    configManager.setPotChannel(slotIndex, midiChannel);
    configManager.setPotCCNumber(slotIndex, data1);
    potentiometerManager.setChannel(slotIndex, midiChannel);
    potentiometerManager.setCCNumber(slotIndex, data1);
    if (static_cast<size_t>(slotIndex) < potChannels.size()) {
        potChannels[slotIndex] = midiChannel;
    }
}

void clearEfSlotAssignments() {
    potToEnvelopeMap.clear();
    for (uint8_t slotIndex = 0; slotIndex < NUM_SLOTS; ++slotIndex) {
        MIDISlot &slot = configManager.getSlot(slotIndex);
        slot.setEnvelopeFollowerIndex(-1);
    }
}

void assignFollowerToSlot(int followerIndex, int slotIndex) {
    if (followerIndex < 0 || followerIndex >= static_cast<int>(envelopeFollowers.size())) {
        return;
    }
    if (slotIndex < 0 || slotIndex >= NUM_POTS) {
        return;
    }
    MIDISlot &slot = configManager.getSlot(static_cast<uint8_t>(slotIndex));
    slot.setEnvelopeFollowerIndex(static_cast<int8_t>(followerIndex));
    potToEnvelopeMap[slotIndex] = slot.efSettings;
}

void applyEfSlotMappingEntry(JsonObject mapping, uint8_t defaultFollowerIndex) {
    if (mapping.isNull()) {
        return;
    }

    int followerIndex = static_cast<int>(defaultFollowerIndex);
    if (mapping.containsKey("index")) {
        followerIndex = mapping["index"].as<int>();
    }
    if (followerIndex < 0 || followerIndex >= static_cast<int>(envelopeFollowers.size())) {
        return;
    }

    if (mapping.containsKey("slots") && mapping["slots"].is<JsonArray>()) {
        JsonArray targets = mapping["slots"].as<JsonArray>();
        for (JsonVariant value : targets) {
            assignFollowerToSlot(followerIndex, value.as<int>());
        }
        return;
    }

    if (mapping.containsKey("slot")) {
        assignFollowerToSlot(followerIndex, mapping["slot"].as<int>());
    }
}

void applyFollowerSettingsFromPotMap() {
    std::array<bool, NUM_ENVELOPES> followerConfigured{};
    followerConfigured.fill(false);

    for (const auto &entry : potToEnvelopeMap) {
        const int followerIndex = entry.second.followerIndex;
        if (followerIndex < 0 || followerIndex >= static_cast<int>(envelopeFollowers.size())) {
            continue;
        }

        envelopeFollowers[followerIndex].setModulationTarget(
            potentiometerManager.getCCNumber(entry.first));
        if (!followerConfigured[followerIndex]) {
            applyEfSettingsToFollower(envelopeFollowers[followerIndex], entry.second,
                                      static_cast<uint8_t>(followerIndex));
            followerConfigured[followerIndex] = true;
        }
    }
}

SlotEnvelopePayload readGlobalFilterPayload(JsonObject filterObj) {
    EnvelopeFollower::FilterType current = envelopeFollowers.empty()
                                               ? EnvelopeFollower::LINEAR
                                               : envelopeFollowers.front().getFilterType();
    EnvelopeFollower::FilterType filterType =
        parseFilterType(filterObj["type"].as<const char *>(), current);
    float freq = constrain(readFloatField(filterObj, "freq", "frequency", EF_FILTER_FREQ_MIN_HZ),
                           EF_FILTER_FREQ_MIN_HZ, EF_FILTER_FREQ_MAX_HZ);
    float q = constrain(readFloatField(filterObj, "q", "q", EF_FILTER_Q_MIN), EF_FILTER_Q_MIN,
                        EF_FILTER_Q_MAX);
    if (filterObj.containsKey("idle_floor") || filterObj.containsKey("idleFloor")) {
        configManager.setEfIdleFloor(readClampedU8(filterObj, "idle_floor", "idleFloor", 0, 127,
                                                   configManager.getEfIdleFloor()));
    }

    SlotEnvelopePayload tailPayload{};
    tailPayload.filterType = static_cast<uint8_t>(filterType);
    tailPayload.frequency = freq;
    tailPayload.q = q;
    return configManager.persistFilterTail(tailPayload);
}

void applyGlobalFilterToFollowers(const SlotEnvelopePayload &payload) {
    EnvelopeFollower::FilterType filterType =
        static_cast<EnvelopeFollower::FilterType>(payload.filterType);
    for (auto &ef : envelopeFollowers) {
        ef.setFilterType(filterType);
        ef.configureFilter(payload.frequency, payload.q);
    }
}

void backfillSlotEnvelopePayloads(const SlotEnvelopePayload &payload) {
    for (uint8_t i = 0; i < NUM_SLOTS; ++i) {
        SlotEnvelopePayload slotPayload = configManager.getSlotEnvelopePayload(i);
        slotPayload.filterType = payload.filterType;
        slotPayload.frequency = payload.frequency;
        slotPayload.q = payload.q;
        configManager.setSlotEnvelopePayload(i, slotPayload);
    }
}

void applyGlobalArgRouting(const SlotARGConfig &defaultArg) {
    envelopeFollowMode = defaultArg.enabled != 0;
    configManager.setARGEnable(defaultArg.enabled);
    configManager.setARGMethod(static_cast<uint8_t>(defaultArg.method));
    configManager.setEnvelopePair(defaultArg.sourceA, defaultArg.sourceB);
    potentiometerManager.setArgEnvelopePair(defaultArg.sourceA, defaultArg.sourceB);
}

void applyGlobalArgFollowerModes(const SlotARGConfig &defaultArg) {
    EnvelopeFollower::ARG_Method followerMethod = toFollowerArgMethod(defaultArg.method);
    for (auto &ef : envelopeFollowers) {
        ef.setARGMethod(followerMethod);
        ef.setMode(envelopeFollowMode ? EnvelopeFollower::ARG : EnvelopeFollower::SEF);
    }
}

void applyEnvelopeModeLabel(JsonObject config) {
    if (config.containsKey("envelopeMode")) {
        updateEnvelopeModeLabel(config["envelopeMode"].as<const char *>());
    }
}

uint8_t readLedBrightness(JsonObject ledObj) {
    uint8_t brightness = ledManager.getBrightness();
    if (ledObj.containsKey("brightness")) {
        brightness = readClampedU8(ledObj, "brightness", "brightness", 0, 255, brightness);
    }
    return std::min<uint8_t>(brightness, BoardPowerProfile::kLedBrightnessCap);
}

CRGB readLedColor(JsonObject ledObj) {
    CRGB color = ledManager.getColor();
    const char *hex = ledObj["color"].as<const char *>();
    if (!hex) {
        hex = ledObj["hex"].as<const char *>();
    }

    if (hex) {
        CRGB parsed;
        if (parseHexColor(hex, parsed)) {
            color = parsed;
        }
        return color;
    }

    if (ledObj.containsKey("rgb") && ledObj["rgb"].is<JsonObject>()) {
        JsonObject rgb = ledObj["rgb"].as<JsonObject>();
        color.r = readClampedU8(rgb, "r", "r", 0, 255, color.r);
        color.g = readClampedU8(rgb, "g", "g", 0, 255, color.g);
        color.b = readClampedU8(rgb, "b", "b", 0, 255, color.b);
    }
    return color;
}

void applyLedMode(JsonObject ledObj) {
    if (!ledObj.containsKey("mode")) {
        return;
    }

    const char *modeStr = ledObj["mode"].as<const char *>();
    LedMode newMode = ledModeFromString(modeStr, configManager.getLedMode());
    configManager.setLedMode(newMode);
    ledAnimator.setMode(newMode);
}

void emitBulkIngestError(const String &ingestError, uint32_t hint) {
    DiagnosticRecord::recordConfigApplyResult(DiagnosticRecord::ConfigApplyStatus::Error, nullptr);
    if (ingestError == "overflow") {
        emitBulkError("overflow", "config payload too large", hint);
    } else if (ingestError == "orphan") {
        emitBulkError("orphan", "chunk missing frame start", hint);
    } else {
        emitBulkError("ingest", "failed to stage chunk", hint);
    }
}

bool parseBulkConfigDocument(StaticJsonDocument<Utility::kMaxBulkConfigSize> &doc) {
    doc.clear();
    DeserializationError err = deserializeJson(doc, bulkConfigAssembler.payload());
    if (err) {
        DiagnosticRecord::recordConfigApplyResult(DiagnosticRecord::ConfigApplyStatus::Error,
                                                  nullptr);
        emitBulkError("parse", err.c_str(), bulkConfigAssembler.sequenceHint());
        bulkConfigAssembler.reset();
        return false;
    }
    return true;
}

bool resolveBulkApplyIdentity(JsonDocument &doc, BulkApplyIdentity &identity) {
    identity.sequence = doc["seq"].as<uint32_t>();
    if (identity.sequence == 0) {
        identity.sequence = bulkConfigAssembler.sequenceHint();
    }

    const char *configId = doc["config_id"] | nullptr;
    if (!configId || configId[0] == '\0') {
        configId = doc["checksum"] | nullptr;
    }

    const String &checksumHint = bulkConfigAssembler.checksumHint();
    if ((!configId || configId[0] == '\0') && checksumHint.length() > 0) {
        identity.configId = checksumHint;
    } else if (configId) {
        identity.configId = configId;
    } else {
        identity.configId = "";
    }

    if (identity.sequence == 0) {
        identity.sequence = lastAckSequence + 1;
    }

    if (identity.configId.length() == 0) {
        DiagnosticRecord::recordConfigApplyResult(DiagnosticRecord::ConfigApplyStatus::Error,
                                                  nullptr);
        emitBulkError("checksum", "missing checksum/config_id", identity.sequence);
        bulkConfigAssembler.reset();
        return false;
    }

    return true;
}

bool emitDuplicateBulkAckIfNeeded(const BulkApplyIdentity &identity) {
    if (identity.sequence != lastAckSequence || lastAckChecksum != identity.configId) {
        return false;
    }

    LOG_PRINTLN(Utility::formatAck(identity.configId.c_str(), identity.sequence,
                                   lastAppliedChecksum.c_str()));
    bulkConfigAssembler.reset();
    return true;
}

void commitBulkApplyAck(const BulkApplyIdentity &identity) {
    lastAckSequence = identity.sequence;
    lastAckChecksum = identity.configId;
    lastAppliedChecksum = appliedStateChecksum();
    DiagnosticRecord::recordConfigApplyResult(DiagnosticRecord::ConfigApplyStatus::Acked,
                                              identity.configId.c_str());
    LOG_PRINTLN(Utility::formatAck(identity.configId.c_str(), identity.sequence,
                                   lastAppliedChecksum.c_str()));
    bulkConfigAssembler.reset();
}

bool applySlotDefinitions(JsonArray slotsJson, uint32_t seq, bool &anySlotPayloadSpecified) {
    anySlotPayloadSpecified = false;
    for (uint8_t i = 0; i < NUM_SLOTS; ++i) {
        JsonObject slotObj = slotsJson[i];
        if (slotObj.isNull()) {
            emitBulkError("slot_null", "slot entry missing", seq);
            return false;
        }

        MIDIMessageType midiType = MIDIMessageType::OFF;
        if (!parseSlotType(slotObj["type"], slotObj["type_name"], midiType)) {
            emitBulkError("slot_type", "unknown slot type", seq);
            return false;
        }

        MIDISlot &slot = configManager.getSlot(i);
        uint8_t midiChannel =
            readClampedU8(slotObj, "midiChannel", "channel", 1, 16, slot.midiChannel);
        uint8_t data1 = readClampedU8(slotObj, "data1", "cc", 0, 127, slot.data1);
        uint8_t arpNote = slot.arpNote;
        if (slotObj.containsKey("arpNote")) {
            arpNote = readClampedU8(slotObj, "arpNote", "arp_note", 0, 127, slot.arpNote);
        } else if (slotObj.containsKey("arp_note")) {
            arpNote = readClampedU8(slotObj, "arpNote", "arp_note", 0, 127, slot.arpNote);
        } else if (midiType == MIDIMessageType::Note) {
            arpNote = data1;
        }
        bool active = slotObj["active"].as<bool>();

        MIDISlot::EfSettings settings = readSlotEfSettings(slotObj, slot);

        slot.type = midiType;
        slot.midiChannel = midiChannel;
        slot.data1 = data1;
        slot.arpNote = arpNote;
        slot.efSettings = settings;
        slot.setEnvelopeFollowerIndex(settings.followerIndex);
        slot.active = active;
        JsonObject slotArgObj =
            slotObj["arg"].is<JsonObject>() ? slotObj["arg"].as<JsonObject>() : JsonObject();
        if (!slotArgObj.isNull()) {
            slot.arg = parseSlotArgConfig(slotArgObj, slot.arg, false);
        }
        if (!applySlotSysExTemplate(slotObj, slot, seq)) {
            return false;
        }
        slot.arg = parseSlotArgConfig(slotArgObj, slot.arg, true);
        configManager.saveSlot(i, slot);
        applySlotEnvelopePayload(slotObj, i, anySlotPayloadSpecified);
        persistSlotPotRouting(i, midiChannel, data1);
    }
    return true;
}

// Validate every slot against a local candidate before changing either the
// runtime slot arena or persistent storage.  SET_ALL used to discover an
// invalid SysEx template only after earlier slots had already been saved.
// This is the first transaction boundary: later commit work only receives a
// document whose complete slot section has passed parsing and validation.
bool validateSlotDefinitions(JsonArray slotsJson, uint32_t seq) {
    for (uint8_t i = 0; i < NUM_SLOTS; ++i) {
        JsonObject slotObj = slotsJson[i];
        if (slotObj.isNull()) {
            emitBulkError("slot_null", "slot entry missing", seq);
            return false;
        }

        MIDIMessageType midiType = MIDIMessageType::OFF;
        if (!parseSlotType(slotObj["type"], slotObj["type_name"], midiType)) {
            emitBulkError("slot_type", "unknown slot type", seq);
            return false;
        }

        MIDISlot candidate = configManager.getSlot(i);
        candidate.type = midiType;
        candidate.midiChannel =
            readClampedU8(slotObj, "midiChannel", "channel", 1, 16, candidate.midiChannel);
        candidate.data1 = readClampedU8(slotObj, "data1", "cc", 0, 127, candidate.data1);
        if (slotObj.containsKey("arpNote")) {
            candidate.arpNote =
                readClampedU8(slotObj, "arpNote", "arp_note", 0, 127, candidate.arpNote);
        } else if (slotObj.containsKey("arp_note")) {
            candidate.arpNote =
                readClampedU8(slotObj, "arpNote", "arp_note", 0, 127, candidate.arpNote);
        } else if (midiType == MIDIMessageType::Note) {
            candidate.arpNote = candidate.data1;
        }
        candidate.efSettings = readSlotEfSettings(slotObj, candidate);
        candidate.setEnvelopeFollowerIndex(candidate.efSettings.followerIndex);
        JsonObject slotArgObj =
            slotObj["arg"].is<JsonObject>() ? slotObj["arg"].as<JsonObject>() : JsonObject();
        candidate.arg = parseSlotArgConfig(slotArgObj, candidate.arg, true);
        if (!applySlotSysExTemplate(slotObj, candidate, seq)) {
            return false;
        }
    }
    return true;
}

void applyEfSlotMappingsFromConfig(JsonObject config) {
    if (!(config.containsKey("efSlots") && config["efSlots"].is<JsonArray>())) {
        return;
    }

    JsonArray efSlots = config["efSlots"].as<JsonArray>();
    clearEfSlotAssignments();

    for (uint8_t i = 0; i < efSlots.size(); ++i) {
        applyEfSlotMappingEntry(efSlots[i].as<JsonObject>(), i);
    }

    applyFollowerSettingsFromPotMap();
    configManager.saveEnvelopeSettings(potToEnvelopeMap, envelopeFollowers);
}

void applyGlobalFilterState(JsonObject config, bool anySlotPayloadSpecified) {
    if (!(config.containsKey("filter") && config["filter"].is<JsonObject>())) {
        return;
    }

    JsonObject filterObj = config["filter"].as<JsonObject>();
    const SlotEnvelopePayload filterPayload = readGlobalFilterPayload(filterObj);
    applyGlobalFilterToFollowers(filterPayload);

    if (!anySlotPayloadSpecified) {
        backfillSlotEnvelopePayloads(filterPayload);
    }
}

void applyGlobalArgAndModeState(JsonObject config, const SlotARGConfig &defaultArg) {
    applyGlobalArgRouting(defaultArg);
    applyGlobalArgFollowerModes(defaultArg);
    applyEnvelopeModeLabel(config);
}

void applyLedStateFromConfig(JsonObject config) {
    if (!(config.containsKey("led") && config["led"].is<JsonObject>())) {
        return;
    }

    JsonObject ledObj = config["led"].as<JsonObject>();
    uint8_t brightness = readLedBrightness(ledObj);
    CRGB color = readLedColor(ledObj);
    ledManager.setBrightness(brightness);
    ledManager.setColor(color);
    configManager.saveLEDSettings(brightness, color);
    applyLedMode(ledObj);
}

bool applyConfigObject(JsonObject config, uint32_t seq) {
    JsonArray slotsJson;
    if (!parseSlotsArray(config, seq, slotsJson)) {
        return false;
    }

    if (!validateSlotDefinitions(slotsJson, seq)) {
        return false;
    }

    StorageBackend *storage = ConfigManager::getStorageBackend();
    if (!storage->supportsTransactions() || !storage->beginTransaction()) {
        emitBulkError("storage_transaction", "atomic storage generation unavailable", seq);
        return false;
    }

    (void)readDefaultEfSettingsFromConfig(config);
    const SlotARGConfig defaultArg = readDefaultArgConfigFromConfig(config);

    auto rollbackRuntimeFromActiveGeneration = [&]() {
        storage->abortTransaction();
        // The staged generation was never activated. Rehydrate the runtime
        // from the still-active generation so a failed Apply has no live
        // effect either.
        restoreActiveProfileRuntime(false);
    };

    bool anySlotPayloadSpecified = false;
    if (!applySlotDefinitions(slotsJson, seq, anySlotPayloadSpecified)) {
        rollbackRuntimeFromActiveGeneration();
        return false;
    }

    configManager.saveConfiguration();
    applyEfSlotMappingsFromConfig(config);
    applyGlobalFilterState(config, anySlotPayloadSpecified);
    applyGlobalArgAndModeState(config, defaultArg);
    applyLedStateFromConfig(config);
    if (!persistActiveProfileSnapshot() || !storage->commitTransaction()) {
        rollbackRuntimeFromActiveGeneration();
        emitBulkError("storage_commit", "configuration generation was not activated", seq);
        return false;
    }
    markAllFilterTuningRemoteControlActive();
    return true;
}
} // namespace

bool parseSlotType(JsonVariantConst typeField, JsonVariantConst typeNameField,
                   MIDIMessageType &type) {
    auto assignFromIntegral = [&](long candidate) {
        if (candidate < static_cast<long>(MIDIMessageType::OFF) ||
            candidate > static_cast<long>(MIDIMessageType::SysEx)) {
            return false;
        }
        type = static_cast<MIDIMessageType>(candidate);
        return true;
    };

    if (!typeField.isNull()) {
        if (typeField.is<const char *>()) {
            if (parseMIDIType(typeField.as<const char *>(), type)) {
                return true;
            }
        } else if (typeField.is<int>() || typeField.is<long>() || typeField.is<short>() ||
                   typeField.is<signed char>()) {
            if (assignFromIntegral(typeField.as<long>())) {
                return true;
            }
        } else if (typeField.is<unsigned char>() || typeField.is<unsigned short>() ||
                   typeField.is<unsigned int>() || typeField.is<unsigned long>()) {
            unsigned long raw = typeField.as<unsigned long>();
            if (raw <= static_cast<unsigned long>(static_cast<long>(MIDIMessageType::SysEx)) &&
                assignFromIntegral(static_cast<long>(raw))) {
                return true;
            }
        } else if (typeField.is<float>() || typeField.is<double>()) {
            double raw = typeField.as<double>();
            if (std::isfinite(raw)) {
                long candidate = static_cast<long>(raw);
                if (static_cast<double>(candidate) == raw && assignFromIntegral(candidate)) {
                    return true;
                }
            }
        }
    }

    if (!typeNameField.isNull() && typeNameField.is<const char *>()) {
        return parseMIDIType(typeNameField.as<const char *>(), type);
    }
    return false;
}

void handleSetAllBulkCommand(const String &command) {
    String chunk = command.substring(8);
    if (chunk.length() == 0) {
        return;
    }

    String ingestError;
    if (!bulkConfigAssembler.ingestChunk(chunk, ingestError)) {
        emitBulkIngestError(ingestError, bulkConfigAssembler.sequenceHint());
        return;
    }

    if (!bulkConfigAssembler.complete()) {
        return;
    }

    auto &doc = bulkApplyDoc;
    if (!parseBulkConfigDocument(doc)) {
        return;
    }

    BulkApplyIdentity identity;
    if (!resolveBulkApplyIdentity(doc, identity)) {
        return;
    }

    if (emitDuplicateBulkAckIfNeeded(identity)) {
        return;
    }

    JsonObject configObj = doc["config"].as<JsonObject>();
    if (!applyConfigObject(configObj, identity.sequence)) {
        DiagnosticRecord::recordConfigApplyResult(DiagnosticRecord::ConfigApplyStatus::Error,
                                                  identity.configId.c_str());
        bulkConfigAssembler.reset();
        return;
    }

    commitBulkApplyAck(identity);
}
