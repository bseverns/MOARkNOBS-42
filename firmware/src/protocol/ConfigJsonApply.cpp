#include "protocol/ConfigJsonApply.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <array>
#include <cmath>
#include <cctype>
#include <cstdint>
#include <cstring>

#include "ARGMixer.h"
#include "BoardPowerProfile.h"
#include "ConfigManager.h"
#include "EfSettingsUtils.h"
#include "EnvelopeFollower.h"
#include "FirmwareState.h"
#include "Globals.h"
#include "Log.h"
#include "Modes.h"
#include "Protocol.h"
#include "Utility.h"
#include "protocol/ProtocolErrors.h"
#include "protocol/SysExTemplateCodec.h"

namespace {
Utility::BulkConfigAssembler bulkConfigAssembler;
uint32_t lastAckSequence = 0;
String lastAckChecksum;

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

bool applyConfigObject(JsonObject config, uint32_t seq) {
    if (config.isNull()) {
        emitBulkError("config_missing", "config object absent", seq);
        return false;
    }

    if (!config.containsKey("slots") || !config["slots"].is<JsonArray>()) {
        emitBulkError("slots_missing", "config.slots missing", seq);
        return false;
    }

    JsonArray slotsJson = config["slots"].as<JsonArray>();
    if (slotsJson.size() != NUM_SLOTS) {
        emitBulkError("slots_size", "unexpected slot count", seq);
        return false;
    }

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

    bool anySlotPayloadSpecified = false;
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
        }
        bool active = slotObj["active"].as<bool>();

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
            if (efObj.containsKey("index")) {
                rawEfIndex = efObj["index"].as<int>();
            }
            if (efObj.containsKey("filter_index")) {
                int idx = constrain(efObj["filter_index"].as<int>(), 0, 6);
                settings.filterType = static_cast<MIDISlot::EfSettings::FilterType>(idx);
            } else if (efObj.containsKey("filter_name")) {
                const char *label = efObj["filter_name"].as<const char *>();
                EnvelopeFollower::FilterType parsed =
                    parseFilterType(label, EnvelopeFollower::filterFromEfType(settings.filterType));
                settings.filterType = fromEnvelopeFilter(parsed);
            } else if (efObj.containsKey("filter")) {
                const char *label = efObj["filter"].as<const char *>();
                EnvelopeFollower::FilterType parsed =
                    parseFilterType(label, EnvelopeFollower::filterFromEfType(settings.filterType));
                settings.filterType = fromEnvelopeFilter(parsed);
            }
            if (efObj.containsKey("frequency")) {
                settings.frequency = efObj["frequency"].as<float>();
            }
            if (efObj.containsKey("q")) {
                settings.q = efObj["q"].as<float>();
            }
            if (efObj.containsKey("oversample")) {
                settings.oversample = readClampedU8(
                    efObj, "oversample", "oversample", static_cast<int>(EF_OVERSAMPLE_MIN),
                    static_cast<int>(EF_OVERSAMPLE_MAX), settings.oversample);
            }
            if (efObj.containsKey("smoothing")) {
                settings.smoothing = efObj["smoothing"].as<float>();
            }
            if (efObj.containsKey("baseline")) {
                settings.baseline = efObj["baseline"].as<float>();
            }
            if (efObj.containsKey("gain")) {
                settings.gain = efObj["gain"].as<float>();
            }
            if (efObj.containsKey("mode")) {
                if (efObj["mode"].is<const char *>()) {
                    settings.efMode = static_cast<uint8_t>(
                        parseEfMode(efObj["mode"].as<const char *>(),
                                    static_cast<EnvelopeFollower::EFMode>(settings.efMode)));
                } else {
                    int raw = efObj["mode"].as<int>();
                    settings.efMode = static_cast<uint8_t>(
                        constrain(raw, 0, static_cast<int>(EnvelopeFollower::EFMode::Follower)));
                }
            }
            if (efObj.containsKey("auto_baseline")) {
                settings.autoBaseline = efObj["auto_baseline"].as<bool>() ? 1 : 0;
            } else if (efObj.containsKey("autoBaseline")) {
                settings.autoBaseline = efObj["autoBaseline"].as<bool>() ? 1 : 0;
            }
            if (efObj.containsKey("auto_gain")) {
                settings.autoGain = efObj["auto_gain"].as<bool>() ? 1 : 0;
            } else if (efObj.containsKey("autoGain")) {
                settings.autoGain = efObj["autoGain"].as<bool>() ? 1 : 0;
            }
            if (efObj.containsKey("attack_ms")) {
                settings.attackMs =
                    readClampedU16(efObj, "attack_ms", "attackMs", static_cast<int>(EF_TIME_MIN_MS),
                                   static_cast<int>(EF_TIME_MAX_MS), settings.attackMs);
            } else if (efObj.containsKey("attackMs")) {
                settings.attackMs =
                    readClampedU16(efObj, "attack_ms", "attackMs", static_cast<int>(EF_TIME_MIN_MS),
                                   static_cast<int>(EF_TIME_MAX_MS), settings.attackMs);
            }
            if (efObj.containsKey("release_ms")) {
                settings.releaseMs = readClampedU16(
                    efObj, "release_ms", "releaseMs", static_cast<int>(EF_TIME_MIN_MS),
                    static_cast<int>(EF_TIME_MAX_MS), settings.releaseMs);
            } else if (efObj.containsKey("releaseMs")) {
                settings.releaseMs = readClampedU16(
                    efObj, "release_ms", "releaseMs", static_cast<int>(EF_TIME_MIN_MS),
                    static_cast<int>(EF_TIME_MAX_MS), settings.releaseMs);
            }
            if (efObj.containsKey("rms_ms")) {
                settings.rmsWindowMs =
                    readClampedU16(efObj, "rms_ms", "rmsWindowMs", static_cast<int>(EF_TIME_MIN_MS),
                                   static_cast<int>(EF_TIME_MAX_MS), settings.rmsWindowMs);
            } else if (efObj.containsKey("rmsWindowMs")) {
                settings.rmsWindowMs =
                    readClampedU16(efObj, "rms_ms", "rmsWindowMs", static_cast<int>(EF_TIME_MIN_MS),
                                   static_cast<int>(EF_TIME_MAX_MS), settings.rmsWindowMs);
            }
            if (efObj.containsKey("baseline_tau_ms")) {
                settings.baselineTauMs = readClampedU16(
                    efObj, "baseline_tau_ms", "baselineTauMs", static_cast<int>(EF_TIME_MIN_MS),
                    static_cast<int>(EF_TIME_MAX_MS), settings.baselineTauMs);
            } else if (efObj.containsKey("baselineTauMs")) {
                settings.baselineTauMs = readClampedU16(
                    efObj, "baseline_tau_ms", "baselineTauMs", static_cast<int>(EF_TIME_MIN_MS),
                    static_cast<int>(EF_TIME_MAX_MS), settings.baselineTauMs);
            }
            if (efObj.containsKey("gain_tau_ms")) {
                settings.gainTauMs = readClampedU16(
                    efObj, "gain_tau_ms", "gainTauMs", static_cast<int>(EF_TIME_MIN_MS),
                    static_cast<int>(EF_TIME_MAX_MS), settings.gainTauMs);
            } else if (efObj.containsKey("gainTauMs")) {
                settings.gainTauMs = readClampedU16(
                    efObj, "gain_tau_ms", "gainTauMs", static_cast<int>(EF_TIME_MIN_MS),
                    static_cast<int>(EF_TIME_MAX_MS), settings.gainTauMs);
            }
            if (efObj.containsKey("gate_threshold")) {
                settings.gateThreshold = readClampedU8(efObj, "gate_threshold", "gateThreshold", 0,
                                                       127, settings.gateThreshold);
            } else if (efObj.containsKey("gateThreshold")) {
                settings.gateThreshold = readClampedU8(efObj, "gate_threshold", "gateThreshold", 0,
                                                       127, settings.gateThreshold);
            }
            if (efObj.containsKey("gate_hysteresis")) {
                settings.gateHysteresis = readClampedU8(efObj, "gate_hysteresis", "gateHysteresis",
                                                        0, 127, settings.gateHysteresis);
            } else if (efObj.containsKey("gateHysteresis")) {
                settings.gateHysteresis = readClampedU8(efObj, "gate_hysteresis", "gateHysteresis",
                                                        0, 127, settings.gateHysteresis);
            }
            if (efObj.containsKey("activity_threshold")) {
                settings.activityThreshold =
                    readClampedU8(efObj, "activity_threshold", "activityThreshold", 0, 127,
                                  settings.activityThreshold);
            } else if (efObj.containsKey("activityThreshold")) {
                settings.activityThreshold =
                    readClampedU8(efObj, "activity_threshold", "activityThreshold", 0, 127,
                                  settings.activityThreshold);
            }
            if (efObj.containsKey("gain_target")) {
                settings.gainTarget =
                    readClampedU8(efObj, "gain_target", "gainTarget", 0, 127, settings.gainTarget);
            } else if (efObj.containsKey("gainTarget")) {
                settings.gainTarget =
                    readClampedU8(efObj, "gain_target", "gainTarget", 0, 127, settings.gainTarget);
            }
        }

        if (rawEfIndex >= 0 && rawEfIndex < static_cast<int>(envelopeFollowers.size())) {
            settings.followerIndex = static_cast<int8_t>(rawEfIndex);
        } else {
            settings.followerIndex = -1;
        }

        slot.type = midiType;
        slot.midiChannel = midiChannel;
        slot.data1 = data1;
        slot.arpNote = arpNote;
        slot.efSettings = settings;
        slot.setEnvelopeFollowerIndex(settings.followerIndex);
        slot.active = active;
        if (slotObj.containsKey("arg") && slotObj["arg"].is<JsonObject>()) {
            JsonObject slotArgObj = slotObj["arg"].as<JsonObject>();
            SlotARGConfig slotArgConfig = slot.arg;
            if (slotArgObj.containsKey("enable")) {
                slotArgConfig.enabled = slotArgObj["enable"].as<bool>() ? 1 : 0;
            } else if (slotArgObj.containsKey("enabled")) {
                slotArgConfig.enabled = slotArgObj["enabled"].as<bool>() ? 1 : 0;
            }
            if (slotArgObj.containsKey("method")) {
                if (slotArgObj["method"].is<const char *>()) {
                    ARGMethod parsed = parseArgMethod(slotArgObj["method"].as<const char *>(),
                                                      slotArgConfig.method);
                    slotArgConfig.method = parsed;
                } else {
                    int raw = slotArgObj["method"].as<int>();
                    raw = constrain(raw, 0, static_cast<int>(ARGMethod::XORR));
                    slotArgConfig.method = static_cast<ARGMethod>(raw);
                }
            }
            if (slotArgObj.containsKey("a")) {
                slotArgConfig.sourceA = static_cast<uint8_t>(
                    constrain(slotArgObj["a"].as<int>(), 0, NUM_ENVELOPES - 1));
            } else if (slotArgObj.containsKey("sourceA")) {
                slotArgConfig.sourceA = static_cast<uint8_t>(
                    constrain(slotArgObj["sourceA"].as<int>(), 0, NUM_ENVELOPES - 1));
            }
            if (slotArgObj.containsKey("b")) {
                slotArgConfig.sourceB = static_cast<uint8_t>(
                    constrain(slotArgObj["b"].as<int>(), 0, NUM_ENVELOPES - 1));
            } else if (slotArgObj.containsKey("sourceB")) {
                slotArgConfig.sourceB = static_cast<uint8_t>(
                    constrain(slotArgObj["sourceB"].as<int>(), 0, NUM_ENVELOPES - 1));
            }
            slot.arg = sanitizeSlotArg(slotArgConfig);
        }
        if (slot.type == MIDIMessageType::SysEx) {
            String templateError;
            if (!parseSysExTemplateField(slotObj["sysexTemplate"], slot, templateError)) {
                emitBulkError("sysex_template", templateError.c_str(), seq);
                return false;
            }
        } else {
            clearSysExTemplate(slot);
        }
        SlotARGConfig slotArg = slot.arg;
        if (slotObj.containsKey("arg") && slotObj["arg"].is<JsonObject>()) {
            JsonObject argObj = slotObj["arg"].as<JsonObject>();
            if (argObj.containsKey("enable")) {
                slotArg.enabled = argObj["enable"].as<bool>() ? 1 : 0;
            } else if (argObj.containsKey("enabled")) {
                slotArg.enabled = argObj["enabled"].as<bool>() ? 1 : 0;
            }
            if (argObj.containsKey("method")) {
                if (argObj["method"].is<const char *>()) {
                    slotArg.method =
                        parseArgMethod(argObj["method"].as<const char *>(), slotArg.method);
                } else {
                    slotArg.method = static_cast<ARGMethod>(constrain(
                        argObj["method"].as<int>(), 0, static_cast<int>(ARGMethod::XORR)));
                }
            }
            if (argObj.containsKey("a")) {
                int rawA = argObj["a"].as<int>();
                int idxA = envelopeIndexFromAnalogPin(rawA);
                if (idxA < 0)
                    idxA = constrain(rawA, 0, NUM_ENVELOPES - 1);
                slotArg.sourceA = static_cast<uint8_t>(idxA);
            }
            if (argObj.containsKey("b")) {
                int rawB = argObj["b"].as<int>();
                int idxB = envelopeIndexFromAnalogPin(rawB);
                if (idxB < 0)
                    idxB = constrain(rawB, 0, NUM_ENVELOPES - 1);
                slotArg.sourceB = static_cast<uint8_t>(idxB);
            }
            slotArg = sanitizeSlotArg(slotArg);
        }
        slot.arg = slotArg;
        configManager.saveSlot(i, slot);

        if (slotObj.containsKey("ef_payload") && slotObj["ef_payload"].is<JsonObject>()) {
            JsonObject efPayload = slotObj["ef_payload"].as<JsonObject>();
            SlotEnvelopePayload payload = configManager.getSlotEnvelopePayload(i);

            if (efPayload.containsKey("type_index")) {
                payload.filterType = static_cast<uint8_t>(efPayload["type_index"].as<int>());
            } else if (efPayload.containsKey("type")) {
                const char *label = efPayload["type"].as<const char *>();
                EnvelopeFollower::FilterType mapped = parseFilterType(
                    label, static_cast<EnvelopeFollower::FilterType>(payload.filterType));
                payload.filterType = static_cast<uint8_t>(mapped);
            }
            if (efPayload.containsKey("freq")) {
                payload.frequency = efPayload["freq"].as<float>();
            }
            if (efPayload.containsKey("q")) {
                payload.q = efPayload["q"].as<float>();
            }

            configManager.setSlotEnvelopePayload(i, payload);
            anySlotPayloadSpecified = true;
        }

        configManager.setPotChannel(i, midiChannel);
        configManager.setPotCCNumber(i, data1);
        potentiometerManager.setChannel(i, midiChannel);
        potentiometerManager.setCCNumber(i, data1);
        if (static_cast<size_t>(i) < potChannels.size()) {
            potChannels[i] = midiChannel;
        }
    }

    configManager.saveConfiguration();

    if (config.containsKey("efSlots") && config["efSlots"].is<JsonArray>()) {
        JsonArray efSlots = config["efSlots"].as<JsonArray>();
        potToEnvelopeMap.clear();

        for (uint8_t slotIndex = 0; slotIndex < NUM_SLOTS; ++slotIndex) {
            MIDISlot &slot = configManager.getSlot(slotIndex);
            slot.setEnvelopeFollowerIndex(-1);
        }

        auto assignFollowerToSlot = [&](int followerIndex, int slotIndex) {
            if (followerIndex < 0 || followerIndex >= static_cast<int>(envelopeFollowers.size())) {
                return;
            }
            if (slotIndex < 0 || slotIndex >= NUM_POTS) {
                return;
            }
            MIDISlot &slot = configManager.getSlot(static_cast<uint8_t>(slotIndex));
            slot.setEnvelopeFollowerIndex(static_cast<int8_t>(followerIndex));
            potToEnvelopeMap[slotIndex] = slot.efSettings;
        };

        for (uint8_t i = 0; i < efSlots.size(); ++i) {
            JsonObject mapping = efSlots[i];
            if (mapping.isNull()) {
                continue;
            }

            int followerIndex = static_cast<int>(i);
            if (mapping.containsKey("index")) {
                followerIndex = mapping["index"].as<int>();
            }
            if (followerIndex < 0 || followerIndex >= static_cast<int>(envelopeFollowers.size())) {
                continue;
            }

            if (mapping.containsKey("slots") && mapping["slots"].is<JsonArray>()) {
                JsonArray targets = mapping["slots"].as<JsonArray>();
                for (JsonVariant value : targets) {
                    assignFollowerToSlot(followerIndex, value.as<int>());
                }
            } else if (mapping.containsKey("slot")) {
                assignFollowerToSlot(followerIndex, mapping["slot"].as<int>());
            }
        }

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
        configManager.saveEnvelopeSettings(potToEnvelopeMap, envelopeFollowers);
    }

    if (config.containsKey("filter") && config["filter"].is<JsonObject>()) {
        JsonObject filterObj = config["filter"].as<JsonObject>();
        EnvelopeFollower::FilterType current = envelopeFollowers.empty()
                                                   ? EnvelopeFollower::LINEAR
                                                   : envelopeFollowers.front().getFilterType();
        EnvelopeFollower::FilterType filterType =
            parseFilterType(filterObj["type"].as<const char *>(), current);
        float freq =
            constrain(readFloatField(filterObj, "freq", "frequency", EF_FILTER_FREQ_MIN_HZ),
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
        SlotEnvelopePayload sanitizedTail = configManager.persistFilterTail(tailPayload);
        filterType = static_cast<EnvelopeFollower::FilterType>(sanitizedTail.filterType);
        freq = sanitizedTail.frequency;
        q = sanitizedTail.q;

        for (auto &ef : envelopeFollowers) {
            ef.setFilterType(filterType);
            ef.configureFilter(freq, q);
        }

        if (!anySlotPayloadSpecified) {
            for (uint8_t i = 0; i < NUM_SLOTS; ++i) {
                SlotEnvelopePayload payload = configManager.getSlotEnvelopePayload(i);
                payload.filterType = static_cast<uint8_t>(filterType);
                payload.frequency = freq;
                payload.q = q;
                configManager.setSlotEnvelopePayload(i, payload);
            }
        }
    }

    envelopeFollowMode = defaultArg.enabled != 0;
    configManager.setARGEnable(defaultArg.enabled);
    configManager.setARGMethod(static_cast<uint8_t>(defaultArg.method));
    configManager.setEnvelopePair(defaultArg.sourceA, defaultArg.sourceB);
    potentiometerManager.setArgEnvelopePair(defaultArg.sourceA, defaultArg.sourceB);

    EnvelopeFollower::ARG_Method followerMethod = toFollowerArgMethod(defaultArg.method);
    for (auto &ef : envelopeFollowers) {
        ef.setARGMethod(followerMethod);
        ef.setMode(envelopeFollowMode ? EnvelopeFollower::ARG : EnvelopeFollower::SEF);
    }

    if (config.containsKey("envelopeMode")) {
        updateEnvelopeModeLabel(config["envelopeMode"].as<const char *>());
    }

    if (config.containsKey("led") && config["led"].is<JsonObject>()) {
        JsonObject ledObj = config["led"].as<JsonObject>();
        uint8_t brightness = ledManager.getBrightness();
        if (ledObj.containsKey("brightness")) {
            brightness = readClampedU8(ledObj, "brightness", "brightness", 0, 255, brightness);
        }
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
        } else if (ledObj.containsKey("rgb") && ledObj["rgb"].is<JsonObject>()) {
            JsonObject rgb = ledObj["rgb"].as<JsonObject>();
            color.r = readClampedU8(rgb, "r", "r", 0, 255, color.r);
            color.g = readClampedU8(rgb, "g", "g", 0, 255, color.g);
            color.b = readClampedU8(rgb, "b", "b", 0, 255, color.b);
        }
        brightness = std::min<uint8_t>(brightness, BoardPowerProfile::kLedBrightnessCap);
        ledManager.setBrightness(brightness);
        ledManager.setColor(color);
        configManager.saveLEDSettings(brightness, color);
        if (ledObj.containsKey("mode")) {
            const char *modeStr = ledObj["mode"].as<const char *>();
            LedMode newMode = ledModeFromString(modeStr, configManager.getLedMode());
            configManager.setLedMode(newMode);
            ledAnimator.setMode(newMode);
        }
    }

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
        uint32_t hint = bulkConfigAssembler.sequenceHint();
        if (ingestError == "overflow") {
            emitBulkError("overflow", "config payload too large", hint);
        } else if (ingestError == "orphan") {
            emitBulkError("orphan", "chunk missing frame start", hint);
        } else {
            emitBulkError("ingest", "failed to stage chunk", hint);
        }
        return;
    }

    if (!bulkConfigAssembler.complete()) {
        return;
    }

    static StaticJsonDocument<Utility::kMaxBulkConfigSize> doc;
    doc.clear();
    DeserializationError err = deserializeJson(doc, bulkConfigAssembler.payload());
    if (err) {
        emitBulkError("parse", err.c_str(), bulkConfigAssembler.sequenceHint());
        bulkConfigAssembler.reset();
        return;
    }

    uint32_t seq = doc["seq"].as<uint32_t>();
    if (seq == 0) {
        seq = bulkConfigAssembler.sequenceHint();
    }
    const char *configId = doc["config_id"] | nullptr;
    if (!configId || configId[0] == '\0') {
        configId = doc["checksum"] | nullptr;
    }
    const String &checksumHint = bulkConfigAssembler.checksumHint();
    if ((!configId || configId[0] == '\0') && checksumHint.length() > 0) {
        configId = checksumHint.c_str();
    }
    if (!configId || configId[0] == '\0') {
        emitBulkError("checksum", "missing checksum/config_id", seq);
        bulkConfigAssembler.reset();
        return;
    }

    if (seq == 0) {
        seq = lastAckSequence + 1;
    }

    if (seq == lastAckSequence && lastAckChecksum == configId) {
        LOG_PRINTLN(Utility::formatAck(configId, seq));
        bulkConfigAssembler.reset();
        return;
    }

    JsonObject configObj = doc["config"].as<JsonObject>();
    if (!applyConfigObject(configObj, seq)) {
        bulkConfigAssembler.reset();
        return;
    }

    lastAckSequence = seq;
    lastAckChecksum = configId;
    LOG_PRINTLN(Utility::formatAck(configId, seq));
    bulkConfigAssembler.reset();
}
