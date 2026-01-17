// firmware_main.cpp is the command center. Beyond spinning the scheduler, these
// comments narrate how every subsystem plugs together—great for walking a class
// through dependency injection, ownership, and why embedded code leans so hard
// on globals for determinism.

#include <Arduino.h>
#include "MIDIHandler.h"
#include "LEDManager.h"
#include "ConfigManager.h"
#include "EnvelopeFollower.h"
#include "DisplayManager.h"
#include "ButtonManager.h"
#include "PotentiometerManager.h"
#include "WebSerial.h"
#include "Utility.h"
#include "Log.h"
#include "TimeUtils.h"
#include "name.c"
#include "Globals.h" // contains all pin definitions
#include "PerlinNoise.h"
#include "ARGMixer.h"
#include "version.h"
#include "BiquadFilter.h"
#include "Arpeggiator.h"
#include "SysExTemplate.h"
#include "interop/SeedBoxLink.h"
#include "LFO/LFOManager.h"
#include <TimerOne.h>
#include <ArduinoJson.h>
#include <queue>
#include <map>
#include <array>
#include <vector>
#include <imxrt.h>
#include <cstdint>
#include <cctype>
#include <cmath>
#include <algorithm>

extern std::vector<uint8_t> potChannels;
extern std::map<int, MIDISlot::EfSettings> potToEnvelopeMap;
extern PotentiometerManager potentiometerManager;
extern std::vector<EnvelopeFollower> envelopeFollowers;
extern LEDManager ledManager;
extern ConfigManager configManager;
extern bool envelopeFollowMode;
extern String g_envelopeModeLabel;
extern const char *envelopeMode;
extern LFOManager lfoManager;

using EfSettings = MIDISlot::EfSettings;

#if defined(ARDUINO)
extern "C" {
extern char _ebss;
extern char _flashimagelen;
extern char _estack;
}
#endif

// Sneaky static that kicks in before setup() even thinks about stretching.
// It pulls in pin maps and timing constants from Globals.h so the rest of this
// file can swagger with real values. If you need to rewrite the defaults,
// hunt down loadHardwareConfig() in firmware/src/Globals.cpp
// and make your mark.
struct HardwareConfigInitializer {
    HardwareConfigInitializer() { loadHardwareConfig(); }
} _hwInit;

uint8_t midiBeatPosition = 0; // 0-7 beat slot; bumps each MIDI clock tick then wraps on the 8th
char serialBuffer[SERIAL_BUFFER_SIZE]; // Holding pen where serial graffiti waits for judgement
uint8_t serialBufferIndex = 0; // Cursor into serialBuffer; resets on newline or when it overflows

namespace {
size_t computeFreeRAM() {
#if defined(ARDUINO)
    char stackDummy = 0;
    uintptr_t stackPtr = reinterpret_cast<uintptr_t>(&stackDummy);
    uintptr_t heapBase = reinterpret_cast<uintptr_t>(&_ebss);
    return (stackPtr > heapBase) ? static_cast<size_t>(stackPtr - heapBase) : 0U;
#else
    return 0U;
#endif
}

size_t computeFreeFlash() {
#if defined(ARDUINO)
    constexpr size_t kFlashSizeBytes =
        1984U * 1024U; // Teensy 4.0 ships with 1.9375 MB of program flash.
    size_t used = reinterpret_cast<uintptr_t>(&_flashimagelen);
    return (used < kFlashSizeBytes) ? (kFlashSizeBytes - used) : 0U;
#else
    return 0U;
#endif
}

void clearSysExTemplate(MIDISlot &slot) {
    slot.sysexLength = 0;
    slot.sysexTemplate.fill(0);
}

bool parseSysExTemplateField(JsonVariantConst value, MIDISlot &slot, String &error) {
    if (value.isNull()) {
        clearSysExTemplate(slot);
        return true;
    }
    const char *raw = value.as<const char *>();
    if (!raw || raw[0] == '\0') {
        clearSysExTemplate(slot);
        return true;
    }
    if (SysExTemplate::parse(raw, slot.sysexTemplate, slot.sysexLength, error)) {
        return true;
    }
    clearSysExTemplate(slot);
    return false;
}

String formatSysExTemplate(const MIDISlot &slot) {
    if (slot.sysexLength == 0) {
        return String();
    }
    return SysExTemplate::format(slot.sysexTemplate, slot.sysexLength);
}

uint8_t buildSysExPayload(const MIDISlot &slot, uint16_t rawValue, uint8_t *dest,
                          std::size_t capacity) {
    const uint8_t value7 = Utility::mapToMidiValue(static_cast<int>(rawValue));
    const uint16_t value14 = Utility::mapTo14Bit(static_cast<int>(rawValue));
    if (slot.sysexLength >= 2) {
        uint8_t rendered = SysExTemplate::render(slot.sysexTemplate, slot.sysexLength, value7,
                                                 value14, dest, capacity);
        if (rendered > 0) {
            return rendered;
        }
    }
    if (capacity < 4) {
        return 0;
    }
    dest[0] = 0xF0;
    dest[1] = slot.data1;
    dest[2] = value7;
    dest[3] = 0xF7;
    return 4;
}

const char *midiMessageTypeName(MIDIMessageType type) {
    switch (type) {
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
    }
    return "UNKNOWN";
}

const char *envelopeFilterName(EnvelopeFollower::FilterType type) {
    switch (type) {
    case EnvelopeFollower::LINEAR:
        return "LINEAR";
    case EnvelopeFollower::OPPOSITE_LINEAR:
        return "OPPOSITE_LINEAR";
    case EnvelopeFollower::EXPONENTIAL:
        return "EXPONENTIAL";
    case EnvelopeFollower::RANDOM:
        return "RANDOM";
    case EnvelopeFollower::LOWPASS:
        return "LOWPASS";
    case EnvelopeFollower::HIGHPASS:
        return "HIGHPASS";
    case EnvelopeFollower::BANDPASS:
        return "BANDPASS";
    default:
        return "CUSTOM";
    }
}

const char *efFilterLabel(MIDISlot::EfSettings::FilterType type) {
    using Filter = MIDISlot::EfSettings::FilterType;
    switch (type) {
    case Filter::Linear:
        return "LINEAR";
    case Filter::OppositeLinear:
        return "OPPOSITE_LINEAR";
    case Filter::Exponential:
        return "EXPONENTIAL";
    case Filter::Random:
        return "RANDOM";
    case Filter::Lowpass:
        return "LOWPASS";
    case Filter::Highpass:
        return "HIGHPASS";
    case Filter::Bandpass:
        return "BANDPASS";
    }
    return "LINEAR";
}

EnvelopeFollower::FilterType toEnvelopeFilter(MIDISlot::EfSettings::FilterType type) {
    using Filter = MIDISlot::EfSettings::FilterType;
    switch (type) {
    case Filter::Linear:
        return EnvelopeFollower::LINEAR;
    case Filter::OppositeLinear:
        return EnvelopeFollower::OPPOSITE_LINEAR;
    case Filter::Exponential:
        return EnvelopeFollower::EXPONENTIAL;
    case Filter::Random:
        return EnvelopeFollower::RANDOM;
    case Filter::Lowpass:
        return EnvelopeFollower::LOWPASS;
    case Filter::Highpass:
        return EnvelopeFollower::HIGHPASS;
    case Filter::Bandpass:
        return EnvelopeFollower::BANDPASS;
    }
    return EnvelopeFollower::LINEAR;
}

MIDISlot::EfSettings::FilterType fromEnvelopeFilter(EnvelopeFollower::FilterType type) {
    switch (type) {
    case EnvelopeFollower::LINEAR:
        return MIDISlot::EfSettings::FilterType::Linear;
    case EnvelopeFollower::OPPOSITE_LINEAR:
        return MIDISlot::EfSettings::FilterType::OppositeLinear;
    case EnvelopeFollower::EXPONENTIAL:
        return MIDISlot::EfSettings::FilterType::Exponential;
    case EnvelopeFollower::RANDOM:
        return MIDISlot::EfSettings::FilterType::Random;
    case EnvelopeFollower::LOWPASS:
        return MIDISlot::EfSettings::FilterType::Lowpass;
    case EnvelopeFollower::HIGHPASS:
        return MIDISlot::EfSettings::FilterType::Highpass;
    case EnvelopeFollower::BANDPASS:
        return MIDISlot::EfSettings::FilterType::Bandpass;
    }
    return MIDISlot::EfSettings::FilterType::Linear;
}

std::array<float, NUM_ENVELOPES> efBaseGains{};

void applyEfSettingsToFollower(EnvelopeFollower &ef, const MIDISlot::EfSettings &settings,
                               uint8_t followerIndex) {
    ef.setFilterType(toEnvelopeFilter(settings.filterType));
    ef.configureFilter(settings.frequency, settings.q);
    ef.setOversampleCount(settings.oversample);
    ef.setSmoothingAlpha(settings.smoothing);
    ef.setBaseline(settings.baseline);
    ef.setGain(settings.gain);
    EnvelopeFollower::EfModeSettings modeSettings{};
    modeSettings.mode = static_cast<EnvelopeFollower::EFMode>(settings.efMode);
    modeSettings.attackMs = settings.attackMs;
    modeSettings.releaseMs = settings.releaseMs;
    modeSettings.rmsWindowMs = settings.rmsWindowMs;
    modeSettings.baselineTauMs = settings.baselineTauMs;
    modeSettings.gainTauMs = settings.gainTauMs;
    modeSettings.gateThreshold = settings.gateThreshold;
    modeSettings.gateHysteresis = settings.gateHysteresis;
    modeSettings.activityThreshold = settings.activityThreshold;
    modeSettings.gainTarget = settings.gainTarget;
    modeSettings.autoBaseline = settings.autoBaseline != 0;
    modeSettings.autoGain = settings.autoGain != 0;
    ef.setModeSettings(modeSettings);
    if (followerIndex < NUM_ENVELOPES) {
        efBaseGains[followerIndex] = settings.gain;
    }
}

ProfileEfSettings profileEfFromSlot(const MIDISlot::EfSettings &settings) {
    // Distill per-slot EF mode settings into the profile format.
    ProfileEfSettings profile{};
    profile.mode = settings.efMode;
    profile.autoBaseline = settings.autoBaseline;
    profile.autoGain = settings.autoGain;
    profile.gateThreshold = settings.gateThreshold;
    profile.gateHysteresis = settings.gateHysteresis;
    profile.activityThreshold = settings.activityThreshold;
    profile.gainTarget = settings.gainTarget;
    profile.attackMs = settings.attackMs;
    profile.releaseMs = settings.releaseMs;
    profile.rmsWindowMs = settings.rmsWindowMs;
    profile.baselineTauMs = settings.baselineTauMs;
    profile.gainTauMs = settings.gainTauMs;
    return profile;
}

void applyProfileEfToSlot(const ProfileEfSettings &profile, MIDISlot::EfSettings &settings) {
    // Update only EF mode parameters, leaving filter/gain/routing intact.
    settings.efMode = profile.mode;
    settings.autoBaseline = profile.autoBaseline;
    settings.autoGain = profile.autoGain;
    settings.gateThreshold = profile.gateThreshold;
    settings.gateHysteresis = profile.gateHysteresis;
    settings.activityThreshold = profile.activityThreshold;
    settings.gainTarget = profile.gainTarget;
    settings.attackMs = profile.attackMs;
    settings.releaseMs = profile.releaseMs;
    settings.rmsWindowMs = profile.rmsWindowMs;
    settings.baselineTauMs = profile.baselineTauMs;
    settings.gainTauMs = profile.gainTauMs;
}

ProfileData captureProfileSnapshot() {
    // Serialize the current runtime state into a profile payload.
    ProfileData profile{};
    profile.routeCount = 0;

    profile.arp.lengthTicks = arpeggiator.getLength();
    profile.arp.shape = static_cast<uint8_t>(arpeggiator.getShape());
    profile.arp.swingPercent =
        static_cast<uint8_t>(constrain(arpeggiator.getSwingPercent(), 0.0f, 80.0f));
    profile.arp.gatePercent =
        static_cast<uint8_t>(constrain(arpeggiator.getGatePercent(), 5.0f, 100.0f));
    profile.arp.octaveRange = arpeggiator.getOctaveRange();

    profile.led.brightness = ledManager.getBrightness();
    CRGB color = ledManager.getColor();
    profile.led.r = color.r;
    profile.led.g = color.g;
    profile.led.b = color.b;

    for (uint8_t i = 0; i < PROFILE_LFO_COUNT; ++i) {
        LFO &lfo = lfoManager.lfo(i);
        profile.lfos[i].shape = static_cast<uint8_t>(lfo.getShape());
        profile.lfos[i].frequencyHz = lfo.getFrequencyHz();
        profile.lfos[i].depth = lfo.getDepth();
        profile.lfos[i].bipolar = lfo.isBipolar() ? 1 : 0;
        profile.lfos[i].syncEnabled = lfo.isSyncEnabled() ? 1 : 0;
        profile.lfos[i].syncRatio = static_cast<uint8_t>(lfo.getSyncRatio());
    }

    const size_t routeCount =
        std::min(lfoManager.routeCount(), static_cast<size_t>(PROFILE_MAX_ROUTES));
    profile.routeCount = static_cast<uint8_t>(routeCount);
    for (size_t i = 0; i < routeCount; ++i) {
        LFOManager::Route route{};
        if (!lfoManager.getRoute(i, route)) {
            continue;
        }
        profile.routes[i].type = static_cast<uint8_t>(route.type);
        profile.routes[i].lfoIndex = route.lfoIndex;
        profile.routes[i].depth = route.depth;
        profile.routes[i].target = static_cast<uint8_t>(route.target);
        profile.routes[i].channel = route.channel;
        profile.routes[i].ccMsb = route.ccMsb;
        profile.routes[i].ccLsb = route.ccLsb;
    }

    const auto &slots = configManager.getSlots();
    for (uint8_t i = 0; i < NUM_SLOTS; ++i) {
        profile.slots[i].midiChannel = slots[i].midiChannel;
        profile.slots[i].ef = profileEfFromSlot(slots[i].efSettings);
    }

    return profile;
}

void applyProfileSnapshot(const ProfileData &profile, bool persistSlots) {
    // Apply profile payload values across arp, LFOs, LEDs, and slot settings.
    arpeggiator.setLength(profile.arp.lengthTicks);
    arpeggiator.setShape(static_cast<Arpeggiator::Shape>(profile.arp.shape));
    arpeggiator.setSwingPercent(profile.arp.swingPercent);
    arpeggiator.setGatePercent(profile.arp.gatePercent);
    arpeggiator.setOctaveRange(profile.arp.octaveRange);

    ledManager.setBrightness(profile.led.brightness);
    ledManager.setColor(CRGB(profile.led.r, profile.led.g, profile.led.b));

    for (uint8_t i = 0; i < PROFILE_LFO_COUNT; ++i) {
        LFO &lfo = lfoManager.lfo(i);
        lfo.setShape(static_cast<LFOShape>(profile.lfos[i].shape));
        lfo.setFrequencyHz(profile.lfos[i].frequencyHz);
        lfo.setDepth(profile.lfos[i].depth);
        lfo.setBipolar(profile.lfos[i].bipolar != 0);
        lfo.setSyncEnabled(profile.lfos[i].syncEnabled != 0);
        lfo.setSyncRatio(static_cast<LFOSyncRatio>(profile.lfos[i].syncRatio));
    }

    lfoManager.clearRoutes();
    for (uint8_t i = 0; i < profile.routeCount && i < PROFILE_MAX_ROUTES; ++i) {
        const ProfileLfoRoute &route = profile.routes[i];
        switch (static_cast<LFOManager::Route::Type>(route.type)) {
        case LFOManager::Route::Type::Internal:
            lfoManager.addInternalRoute(route.lfoIndex,
                                        static_cast<LFOInternalTarget>(route.target), route.depth);
            break;
        case LFOManager::Route::Type::MidiCC7:
            lfoManager.addMidiCC7Route(route.lfoIndex, route.ccMsb, route.channel, route.depth);
            break;
        case LFOManager::Route::Type::MidiCC14:
            lfoManager.addMidiCC14Route(route.lfoIndex, route.ccMsb, route.ccLsb, route.channel,
                                        route.depth);
            break;
        case LFOManager::Route::Type::Osc:
            lfoManager.addOscRoute(route.lfoIndex, route.depth);
            break;
        }
    }

    for (uint8_t i = 0; i < NUM_SLOTS; ++i) {
        MIDISlot &slot = configManager.getSlot(i);
        slot.midiChannel = profile.slots[i].midiChannel;
        applyProfileEfToSlot(profile.slots[i].ef, slot.efSettings);
        if (persistSlots) {
            configManager.saveSlot(i, slot);
        }
        if (slot.getEnvelopeFollowerIndex() >= 0) {
            potToEnvelopeMap[i] = slot.efSettings;
        }
    }

    for (const auto &entry : potToEnvelopeMap) {
        const int potIndex = entry.first;
        const int followerIndex = entry.second.followerIndex;
        if (followerIndex >= 0 && followerIndex < static_cast<int>(envelopeFollowers.size())) {
            applyEfSettingsToFollower(envelopeFollowers[followerIndex], entry.second,
                                      static_cast<uint8_t>(followerIndex));
        }
    }

    if (persistSlots) {
        configManager.saveLEDSettings(ledManager.getBrightness(), ledManager.getColor());
        configManager.saveEnvelopeSettings(potToEnvelopeMap, envelopeFollowers);
    }
}

const char *argMethodName(uint8_t method) {
    static constexpr const char *kNames[] = {"PLUS", "MIN",  "PECK", "SHAV", "SQAR",
                                             "BABS", "TABS", "MULT", "DIVI", "AVG",
                                             "XABS", "MAXX", "MINN", "XORR"};
    if (method < (sizeof(kNames) / sizeof(kNames[0]))) {
        return kNames[method];
    }
    return "UNKNOWN";
}

const char *envelopeModeName(uint8_t mode) { return (mode != 0) ? "ARG" : "SEF"; }
} // namespace

void refreshEfVoicesFromConfig();

namespace {
Utility::BulkConfigAssembler bulkConfigAssembler;
uint32_t lastAckSequence = 0;
String lastAckChecksum;

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

EnvelopeFollower::FilterType parseFilterType(const char *label,
                                             EnvelopeFollower::FilterType fallback);
EnvelopeFollower::EFMode parseEfMode(const char *label, EnvelopeFollower::EFMode fallback);

EnvelopeFollower::FilterType decodeFilterType(EfSettings::FilterType raw) {
    return static_cast<EnvelopeFollower::FilterType>(raw);
}

EfSettings::FilterType encodeFilterType(EnvelopeFollower::FilterType type) {
    return static_cast<EfSettings::FilterType>(type);
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
        int oversample = obj["oversample"].as<int>();
        out.oversample = static_cast<uint8_t>(constrain(oversample, 1, 32));
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

    // EF mode and auto-cal settings accept both snake_case and camelCase keys.
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
        out.attackMs = static_cast<uint16_t>(obj["attack_ms"].as<int>());
    } else if (obj.containsKey("attackMs")) {
        out.attackMs = static_cast<uint16_t>(obj["attackMs"].as<int>());
    }

    if (obj.containsKey("release_ms")) {
        out.releaseMs = static_cast<uint16_t>(obj["release_ms"].as<int>());
    } else if (obj.containsKey("releaseMs")) {
        out.releaseMs = static_cast<uint16_t>(obj["releaseMs"].as<int>());
    }

    if (obj.containsKey("rms_ms")) {
        out.rmsWindowMs = static_cast<uint16_t>(obj["rms_ms"].as<int>());
    } else if (obj.containsKey("rmsWindowMs")) {
        out.rmsWindowMs = static_cast<uint16_t>(obj["rmsWindowMs"].as<int>());
    }

    if (obj.containsKey("baseline_tau_ms")) {
        out.baselineTauMs = static_cast<uint16_t>(obj["baseline_tau_ms"].as<int>());
    } else if (obj.containsKey("baselineTauMs")) {
        out.baselineTauMs = static_cast<uint16_t>(obj["baselineTauMs"].as<int>());
    }

    if (obj.containsKey("gain_tau_ms")) {
        out.gainTauMs = static_cast<uint16_t>(obj["gain_tau_ms"].as<int>());
    } else if (obj.containsKey("gainTauMs")) {
        out.gainTauMs = static_cast<uint16_t>(obj["gainTauMs"].as<int>());
    }

    if (obj.containsKey("gate_threshold")) {
        out.gateThreshold = static_cast<uint8_t>(obj["gate_threshold"].as<int>());
    } else if (obj.containsKey("gateThreshold")) {
        out.gateThreshold = static_cast<uint8_t>(obj["gateThreshold"].as<int>());
    }

    if (obj.containsKey("gate_hysteresis")) {
        out.gateHysteresis = static_cast<uint8_t>(obj["gate_hysteresis"].as<int>());
    } else if (obj.containsKey("gateHysteresis")) {
        out.gateHysteresis = static_cast<uint8_t>(obj["gateHysteresis"].as<int>());
    }

    if (obj.containsKey("activity_threshold")) {
        out.activityThreshold = static_cast<uint8_t>(obj["activity_threshold"].as<int>());
    } else if (obj.containsKey("activityThreshold")) {
        out.activityThreshold = static_cast<uint8_t>(obj["activityThreshold"].as<int>());
    }

    if (obj.containsKey("gain_target")) {
        out.gainTarget = static_cast<uint8_t>(obj["gain_target"].as<int>());
    } else if (obj.containsKey("gainTarget")) {
        out.gainTarget = static_cast<uint8_t>(obj["gainTarget"].as<int>());
    }

    if (obj.containsKey("index")) {
        int rawIndex = obj["index"].as<int>();
        rawIndex = constrain(rawIndex, -1, static_cast<int>(NUM_ENVELOPES) - 1);
        out.followerIndex = static_cast<int8_t>(rawIndex);
    }
}

bool parseProfileEf(JsonObject obj, ProfileEfSettings &out) {
    // Apply EF profile fields from JSON, keeping unspecified values intact.
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

void writeProfileEf(JsonObject obj, const ProfileEfSettings &settings) {
    // Serialize EF profile settings into JSON for the web configurator.
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

void emitBulkError(const char *code, const char *message, uint32_t seq = 0) {
    String out = "{\"type\":\"error\"";
    if (code && code[0] != '\0') {
        out += ",\"code\":\"";
        out += code;
        out += "\"";
    }
    if (seq != 0) {
        out += ",\"seq\":";
        out += seq;
    }
    if (message && message[0] != '\0') {
        out += ",\"message\":\"";
        out += message;
        out += "\"";
    }
    out += "}";
    LOG_PRINTLN(out);
}

void updateEnvelopeModeLabel(const char *label) {
    if (!label || label[0] == '\0') {
        g_envelopeModeLabel = "LINEAR";
    } else {
        g_envelopeModeLabel = label;
    }
    envelopeMode = g_envelopeModeLabel.c_str();
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

        uint8_t midiChannel = constrain(slotObj["midiChannel"].as<int>(), 1, 16);
        uint8_t data1 = constrain(slotObj["data1"].as<int>(), 0, 127);
        bool active = slotObj["active"].as<bool>();

        MIDISlot &slot = configManager.getSlot(i);
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
                    parseFilterType(label, toEnvelopeFilter(settings.filterType));
                settings.filterType = fromEnvelopeFilter(parsed);
            } else if (efObj.containsKey("filter")) {
                const char *label = efObj["filter"].as<const char *>();
                EnvelopeFollower::FilterType parsed =
                    parseFilterType(label, toEnvelopeFilter(settings.filterType));
                settings.filterType = fromEnvelopeFilter(parsed);
            }
            if (efObj.containsKey("frequency")) {
                settings.frequency = efObj["frequency"].as<float>();
            }
            if (efObj.containsKey("q")) {
                settings.q = efObj["q"].as<float>();
            }
            if (efObj.containsKey("oversample")) {
                int oversample = efObj["oversample"].as<int>();
                settings.oversample = static_cast<uint8_t>(constrain(oversample, 1, 32));
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
                settings.attackMs = static_cast<uint16_t>(efObj["attack_ms"].as<int>());
            } else if (efObj.containsKey("attackMs")) {
                settings.attackMs = static_cast<uint16_t>(efObj["attackMs"].as<int>());
            }
            if (efObj.containsKey("release_ms")) {
                settings.releaseMs = static_cast<uint16_t>(efObj["release_ms"].as<int>());
            } else if (efObj.containsKey("releaseMs")) {
                settings.releaseMs = static_cast<uint16_t>(efObj["releaseMs"].as<int>());
            }
            if (efObj.containsKey("rms_ms")) {
                settings.rmsWindowMs = static_cast<uint16_t>(efObj["rms_ms"].as<int>());
            } else if (efObj.containsKey("rmsWindowMs")) {
                settings.rmsWindowMs = static_cast<uint16_t>(efObj["rmsWindowMs"].as<int>());
            }
            if (efObj.containsKey("baseline_tau_ms")) {
                settings.baselineTauMs = static_cast<uint16_t>(efObj["baseline_tau_ms"].as<int>());
            } else if (efObj.containsKey("baselineTauMs")) {
                settings.baselineTauMs = static_cast<uint16_t>(efObj["baselineTauMs"].as<int>());
            }
            if (efObj.containsKey("gain_tau_ms")) {
                settings.gainTauMs = static_cast<uint16_t>(efObj["gain_tau_ms"].as<int>());
            } else if (efObj.containsKey("gainTauMs")) {
                settings.gainTauMs = static_cast<uint16_t>(efObj["gainTauMs"].as<int>());
            }
            if (efObj.containsKey("gate_threshold")) {
                settings.gateThreshold = static_cast<uint8_t>(efObj["gate_threshold"].as<int>());
            } else if (efObj.containsKey("gateThreshold")) {
                settings.gateThreshold = static_cast<uint8_t>(efObj["gateThreshold"].as<int>());
            }
            if (efObj.containsKey("gate_hysteresis")) {
                settings.gateHysteresis = static_cast<uint8_t>(efObj["gate_hysteresis"].as<int>());
            } else if (efObj.containsKey("gateHysteresis")) {
                settings.gateHysteresis = static_cast<uint8_t>(efObj["gateHysteresis"].as<int>());
            }
            if (efObj.containsKey("activity_threshold")) {
                settings.activityThreshold =
                    static_cast<uint8_t>(efObj["activity_threshold"].as<int>());
            } else if (efObj.containsKey("activityThreshold")) {
                settings.activityThreshold =
                    static_cast<uint8_t>(efObj["activityThreshold"].as<int>());
            }
            if (efObj.containsKey("gain_target")) {
                settings.gainTarget = static_cast<uint8_t>(efObj["gain_target"].as<int>());
            } else if (efObj.containsKey("gainTarget")) {
                settings.gainTarget = static_cast<uint8_t>(efObj["gainTarget"].as<int>());
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
        slot.efSettings = settings;
        slot.setEnvelopeFollowerIndex(settings.followerIndex);
        slot.active = active;
        slot.arg = defaultArg;
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
        SlotARGConfig slotArg = defaultArg;
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
        for (uint8_t i = 0; i < efSlots.size(); ++i) {
            JsonObject mapping = efSlots[i];
            if (mapping.isNull())
                continue;
            int slotIndex = mapping["slot"].as<int>();
            if (slotIndex < 0 || slotIndex >= NUM_POTS)
                continue;

            MIDISlot &slot = configManager.getSlot(static_cast<uint8_t>(slotIndex));
            slot.setEnvelopeFollowerIndex(static_cast<int8_t>(i));
            potToEnvelopeMap[slotIndex] = slot.efSettings;

            if (i < envelopeFollowers.size()) {
                envelopeFollowers[i].setModulationTarget(
                    potentiometerManager.getCCNumber(slotIndex));
                applyEfSettingsToFollower(envelopeFollowers[i], slot.efSettings, i);
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
        float freq = constrain(filterObj["freq"].as<float>(), 20.0f, 5000.0f);
        float q = constrain(filterObj["q"].as<float>(), 0.5f, 4.0f);

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
        uint8_t brightness = constrain(ledObj["brightness"].as<int>(), 0, 255);
        CRGB color = ledManager.getColor();
        const char *hex = ledObj["color"].as<const char *>();
        if (hex) {
            CRGB parsed;
            if (parseHexColor(hex, parsed)) {
                color = parsed;
            }
        }
        ledManager.setBrightness(brightness);
        ledManager.setColor(color);
        configManager.saveLEDSettings(brightness, color);
    }

    return true;
}
} // namespace

namespace {

BiquadFilter::FilterType toBiquadType(EnvelopeFollower::FilterType type) {
    switch (type) {
    case EnvelopeFollower::LOWPASS:
        return BiquadFilter::LOWPASS;
    case EnvelopeFollower::HIGHPASS:
        return BiquadFilter::HIGHPASS;
    case EnvelopeFollower::BANDPASS:
        return BiquadFilter::BANDPASS;
    default:
        return BiquadFilter::LOWPASS;
    }
}

float jitterRateFromSmoothness(float smoothness) {
    float clamped = constrain(smoothness, 0.0f, 1.0f);
    return 0.05f + (1.0f - clamped) * 1.95f;
}

float jitterDepth() { return constrain(g_jitterSettings.depth, 0.0f, 1.0f); }

struct EfVoice {
    uint8_t followerIndex = 0xFF; //!< Physical follower index we mirror
    EnvelopeFollower::FilterType filterType = EnvelopeFollower::LINEAR;
    float frequency = 1000.0f;
    float q = 0.707f;
    bool filterDirty = true;
    bool hasFollower = false;
    uint8_t lastLevel = 0;
    bool hasLevel = false;
    BiquadFilter filter;

    void resetFollower() {
        followerIndex = 0xFF;
        hasFollower = false;
        hasLevel = false;
        filterDirty = true;
    }

    void assignFollower(int index) {
        if (index < 0) {
            resetFollower();
            return;
        }
        uint8_t next = static_cast<uint8_t>(index);
        if (!hasFollower || followerIndex != next) {
            followerIndex = next;
            hasFollower = true;
            hasLevel = false;
            filterDirty = true;
        }
    }

    void syncSettings(const EfSettings &settings) {
        EnvelopeFollower::FilterType nextType = decodeFilterType(settings.filterType);
        float nextFreq = settings.frequency;
        float nextQ = settings.q;
        if (!hasFollower) {
            filterType = nextType;
            frequency = nextFreq;
            q = nextQ;
            filterDirty = true;
            return;
        }
        if (filterType != nextType || frequency != nextFreq || q != nextQ) {
            filterType = nextType;
            frequency = nextFreq;
            q = nextQ;
            filterDirty = true;
        }
    }

    uint8_t render(int rawLevel) {
        if (!hasFollower) {
            hasLevel = false;
            lastLevel = 0;
            return 0;
        }

        int level = constrain(rawLevel, 0, 127);
        uint8_t shaped = 0;

        switch (filterType) {
        case EnvelopeFollower::LOWPASS:
        case EnvelopeFollower::HIGHPASS:
        case EnvelopeFollower::BANDPASS: {
            if (filterDirty) {
                filter.configure(toBiquadType(filterType), frequency, 44100.0f, q);
                filterDirty = false;
            }
            float processed = filter.process(static_cast<float>(level));
            shaped = static_cast<uint8_t>(constrain(static_cast<int>(roundf(processed)), 0, 127));
            break;
        }

        case EnvelopeFollower::OPPOSITE_LINEAR: {
            float scaled = static_cast<float>(level) * (frequency / 1000.0f);
            shaped = static_cast<uint8_t>(constrain(127 - static_cast<int>(scaled), 0, 127));
            break;
        }

        case EnvelopeFollower::EXPONENTIAL: {
            float ratio = level / 127.0f;
            float curved = powf(ratio, q) * (frequency / 1000.0f) * 127.0f;
            shaped = static_cast<uint8_t>(constrain(static_cast<int>(roundf(curved)), 0, 127));
            break;
        }

        case EnvelopeFollower::RANDOM: {
            int probability = map(static_cast<int>(frequency), 20, 5000, 0, 100);
            if (probability < 0)
                probability = 0;
            if (probability > 100)
                probability = 100;
            if (random(0, 100) < probability) {
                int range = map(static_cast<int>(q * 100.0f), 50, 400, 1, 64);
                float depth = jitterDepth();
                if (depth <= 0.0f) {
                    shaped = static_cast<uint8_t>(level);
                    break;
                }
                float rate = jitterRateFromSmoothness(g_jitterSettings.smoothness);
                float t = (static_cast<float>(now()) * 0.001f * rate) +
                          (static_cast<float>(followerIndex) * 17.23f);
                float n = perlinNoise1D(t);
                int swing = constrain(range, 1, 64);
                int jitter = static_cast<int>(roundf(n * static_cast<float>(swing) * depth));
                shaped = static_cast<uint8_t>(constrain(level + jitter, 0, 127));
            } else {
                shaped = static_cast<uint8_t>(level);
            }
            break;
        }

        case EnvelopeFollower::LINEAR:
        default: {
            float scaled = static_cast<float>(level) * (frequency / 1000.0f);
            shaped = static_cast<uint8_t>(constrain(static_cast<int>(roundf(scaled)), 0, 127));
            break;
        }
        }

        lastLevel = shaped;
        hasLevel = true;
        return shaped;
    }

    uint8_t latestLevel() const { return hasLevel ? lastLevel : 0; }
    bool hasRendered() const { return hasLevel; }
};

} // namespace

#if defined(UNIT_TEST)
bool testOnly_parseSlotType(JsonVariantConst typeField, JsonVariantConst typeNameField,
                            MIDIMessageType &type) {
    return parseSlotType(typeField, typeNameField, type);
}

bool testOnly_parseSysExTemplateField(JsonVariantConst value, MIDISlot &slot, String &error) {
    return parseSysExTemplateField(value, slot, error);
}

uint8_t testOnly_buildSysExPayload(const MIDISlot &slot, uint16_t rawValue, uint8_t *dest,
                                   size_t capacity) {
    return buildSysExPayload(slot, rawValue, dest, capacity);
}
#endif

// Global objects
std::vector<uint8_t> potChannels; // 42-slot table: each entry stores a slot's MIDI channel
std::map<int, MIDISlot::EfSettings>
    potToEnvelopeMap;            // Crosswalk from pot index to its envelope follower partner
std::queue<String> commandQueue; // Serial command backlog waiting for mid-tier processing
MIDIHandler midiHandler;         // Central MIDI traffic cop slinging bytes over USB + DIN
LEDManager ledManager(hwConfig); // Whips the WS2812 strip into obedient patterns
DisplayManager displayManager(SSD1306_I2C_ADDRESS, 128, 64); // Bosses around the 128x64 OLED
ConfigManager configManager(NUM_POTS, NUM_BUTTONS); // Persists slot + button config to EEPROM
BiquadFilter filter;     // Shared filter template for envelope follower shaping
TaskScheduler scheduler; // Legacy scheduler kept for posterity (most work lives in Utility)
Arpeggiator arpeggiator; // Keeps notes chugging along in time
LFOManager lfoManager;   // Dual LFO engine with routing hooks
std::array<EfVoice, NUM_SLOTS> efVoices; // Software voices riding raw envelope reads per slot

void refreshEfVoicesFromConfig() {
    for (uint8_t slotIndex = 0; slotIndex < NUM_SLOTS; ++slotIndex) {
        EfVoice &voice = efVoices[slotIndex];
        const MIDISlot &slot = configManager.getSlot(slotIndex);
        auto mapIt = potToEnvelopeMap.find(slotIndex);
        if (mapIt != potToEnvelopeMap.end()) {
            voice.assignFollower(mapIt->second.followerIndex);
        } else {
            voice.resetFollower();
        }
        voice.syncSettings(slot.efSettings);
    }
}

// Declare PotentiometerManager before ButtonManager
// Pin 6 is reserved for the LED strip
// Control buttons are direct-wired (not part of the mux matrix)
// and must not share the mux select pins.
const uint8_t controlPins[NUM_CONTROL_BUTTONS] = {12, 13, 14,
                                                  15, 24, 25}; // Direct-wired control buttons
PotentiometerManager potentiometerManager(primaryMuxPins, secondaryMuxPins,
                                          potMuxAnalogPin); // Scans pot muxes & EEPROM slots
ButtonManager buttonManager(hwConfig, controlPins,
                            &potentiometerManager); // Wrangles the button matrix

// Envelope followers – six ADC spies that turn audio/CV into modulation
std::vector<EnvelopeFollower> envelopeFollowers = {
    EnvelopeFollower(A0, &potentiometerManager, 0), EnvelopeFollower(A1, &potentiometerManager, 1),
    EnvelopeFollower(A2, &potentiometerManager, 2), EnvelopeFollower(A3, &potentiometerManager, 3),
    EnvelopeFollower(A6, &potentiometerManager, 4), EnvelopeFollower(A7, &potentiometerManager, 5),
};

// Hardware/UI state trackers
uint8_t activePot = 0xFF;        // Current slot index; 0xFF means "none selected"
uint8_t activeChannel = 1;       // MIDI channel currently under the spotlight
bool envelopeFollowMode = false; // True when EFs are allowed to hijack a slot
String g_envelopeModeLabel = "LINEAR";
const char *envelopeMode = g_envelopeModeLabel.c_str();
int NORMAL_DISPLAY_TIME = 30000; // ms duration for full-size messages
int SHORT_DISPLAY_TIME = 10000;  // ms duration for terse status flashes
bool diagnosticMode = false;     // Self-test mode flag
uint8_t diagnosticPage = 0;      // Active diagnostic page

// Timers for processing
unsigned long lastMIDIProcess = 0;
unsigned long lastSerialProcess = 0;
unsigned long lastLEDUpdate = 0;
unsigned long lastEnvelopeProcess = 0;
unsigned long lastDisplayUpdate = 0;

// ButtonManagerContext: glue struct passed around to avoid global rummaging
ButtonManagerContext buttonContext = {potChannels,        activePot,      activeChannel,
                                      envelopeFollowMode, envelopeMode,   configManager,
                                      ledManager,         displayManager, envelopeFollowers,
                                      potToEnvelopeMap,   diagnosticMode, diagnosticPage};

namespace {
volatile unsigned long statusLedPulseDeadline = 0;

void requestStatusLEDPulse(uint16_t durationMs = 200) {
    statusLedPulseDeadline = now() + durationMs;
}

void serviceStatusLEDPulse() {
    unsigned long deadline = statusLedPulseDeadline;
    if (deadline == 0) {
        ledManager.setStatusLED(false);
        return;
    }
    unsigned long current = now();
    if (static_cast<long>(current - deadline) >= 0) {
        statusLedPulseDeadline = 0;
        ledManager.setStatusLED(false);
    } else {
        ledManager.setStatusLED(true);
    }
}

void monitorSerialHealth() {
#if defined(__IMXRT1062__)
    if (IMXRT_LPUART6.STAT & LPUART_STAT_OR) {
        IMXRT_LPUART6.STAT |= LPUART_STAT_OR;
        ++g_systemDiagnostics.uartOverrunCount;
    }
#endif
}

static SystemDiagnostics captureDiagnosticsSnapshot() {
    SystemDiagnostics snapshot;
    noInterrupts();
    snapshot = g_systemDiagnostics;
    interrupts();
    return snapshot;
}

void checkDiagnosticsForAlerts() {
    static uint32_t lastMidiDrop = 0;
    static uint32_t lastMidiTaskOverrun = 0;
    static uint32_t lastUartOverrun = 0;

    const SystemDiagnostics diag = captureDiagnosticsSnapshot();

    const uint32_t midiDrop = diag.midiDropCount;
    if (midiDrop != lastMidiDrop) {
        LOG_PRINTF("{\"diagnostic\":\"midi_drop\",\"count\":%lu}\n",
                   static_cast<unsigned long>(midiDrop));
        requestStatusLEDPulse();
        lastMidiDrop = midiDrop;
    }

    const uint32_t midiTaskOverruns = diag.midiTaskOverrunCount;
    const uint32_t maxMidiMicros = diag.maxProcessMidiMicros;
    if (midiTaskOverruns != lastMidiTaskOverrun) {
        LOG_PRINTF("{\"diagnostic\":\"midi_task_overrun\",\"count\":%lu,\"max_us\":%lu}\n",
                   static_cast<unsigned long>(midiTaskOverruns),
                   static_cast<unsigned long>(maxMidiMicros));
        requestStatusLEDPulse();
        lastMidiTaskOverrun = midiTaskOverruns;
    }

    const uint32_t uartOverruns = diag.uartOverrunCount;
    if (uartOverruns != lastUartOverrun) {
        LOG_PRINTF("{\"diagnostic\":\"uart_overrun\",\"count\":%lu}\n",
                   static_cast<unsigned long>(uartOverruns));
        requestStatusLEDPulse();
        lastUartOverrun = uartOverruns;
    }
}
} // namespace

void processMIDI() {
    midiHandler.processIncomingMIDI();

    static uint32_t lastDisplayTick = 0;
    uint32_t tickCount = midiHandler.clockTickCount();
    if (tickCount != lastDisplayTick) {
        uint32_t diff = tickCount - lastDisplayTick;
        lastDisplayTick = tickCount;

        lastClockTime = now();
        // Advance beat by however many ticks landed since the last pass
        midiBeatPosition = (midiBeatPosition + diff) % 8;

        // Perform clock-tied updates
        displayManager.updateDisplay(midiBeatPosition,
                                     std::vector<uint8_t>(), // Pass envelope levels if applicable
                                     envelopeFollowMode ? "EF ON" : "EF OFF", activePot,
                                     activeChannel, envelopeMode);

        // Record the last time a clock tick landed
        lastClockTime = now();

        midiHandler.clearClockTick();
    }

    monitorSerialHealth();
}

/*
 * Serial command rodeo — every lasso ends with a newline:
 *   HELLO                             : kick off WebSerial streaming
 *   GET_SCHEMA                        : cough up the config schema
 *   GET_BROWNOUTS                     : report how many times power sagged
 *   SET_POT,<slot>,<chan>,<cc>        : bind slot to MIDI channel+CC
 *   SET_ALL,<payload>                 : blast a JSON blob or bulk slot dump
 *   GET_ALL                           : dump every slot and LED setting
 *   GET_LED                           : spit back brightness,r,g,b
 *   SET_LED,<bri>,<r>,<g>,<b>         : 0‑255 each, paints the strip
 *   GET_ARGMETHOD                     : report current ARG blend
 *   SET_ARGMETHOD,<n>                 : n=0‑6 picks the blend
 *   GET_EF,<slot>                     : who’s modding that slot (‑1 means none)
 *   SET_EF,<slot>,<ef>                : patch an envelope follower
 *   CAL_ENVS                          : recalibrate all envelope spies
 *   GET_PROFILE,<id>                  : dump profile payload as JSON (id 0-3)
 *   SET_PROFILE,<id>,<payload>        : store profile payload JSON (id 0-3)
 *   GET_FILTER                        : reply with type,freq,q for EF filter
 *   SET_FILTER,<type>,<freq>,<q>      : stash envelope filter settings
 *   GET_SLOT_FILTER <slot>            : read one slot's EF payload as type,freq,q
 *   SET_SLOT_FILTER <slot>,<type>,<freq>,<q>
 *                                     : surgically update a single slot's EF payload
 *   GET_ARGPAIR                       : echo ARG pair enable,envA,envB
 *   SET_ARGPAIR,<on>,<envA>,<envB>    : wire two envelopes together
 */
void processSerial() {
    while (Serial.available()) {
        char received = Serial.read();

        if (received == '\n' || serialBufferIndex >= SERIAL_BUFFER_SIZE - 1) {
            serialBuffer[serialBufferIndex] = '\0';
            if (serialBufferIndex >= SERIAL_BUFFER_SIZE - 1) {
                LOG_PRINTLN("Error: Command too long");
            }
            commandQueue.push(String(serialBuffer));
            serialBufferIndex = 0;
        } else if (received != '\r') {
            serialBuffer[serialBufferIndex++] = received;
        }
    }

    // Process queued commands
    while (!commandQueue.empty()) {
        String command = commandQueue.front(); // Get the front command
        commandQueue.pop();                    // Remove it from the queue

        command.trim();

        if (command == "HELLO") {
            webSerialStreaming = true;
            LOG_PRINTLN("{\"hello\":\"mn42\"}");

        } else if (command == "GET_SCHEMA") {
            LOG_PRINTLN(ConfigManager::makeSchema());

        } else if (command == "GET_BROWNOUTS") {
            LOG_PRINTLN(g_brownoutCount);

        } else if (command == "GET_MANIFEST") {
            StaticJsonDocument<256> doc;
            doc["fw_version"] = FW_VERSION_STR;
            doc["git_sha"] = GIT_SHA_STR;
            doc["build_time"] = __DATE__ " " __TIME__;
            doc["schema_version"] = CONFIG_VERSION;
            doc["slot_count"] = NUM_SLOTS;
            doc["pot_count"] = configManager.getNumPots();
            doc["envelope_count"] = NUM_ENVELOPES;
            doc["arg_method_count"] = static_cast<uint8_t>(ARGMethod::XORR) + 1;
            doc["led_count"] = NUM_LEDS();
            doc["free_ram"] = computeFreeRAM();
            doc["free_flash"] = computeFreeFlash();

            String payload;
            serializeJson(doc, payload);
            LOG_PRINTLN(payload);

        } else if (command == "GET_CONFIG") {
            StaticJsonDocument<8192> doc;

            doc["fw_version"] = FW_VERSION_STR;
            doc["schema_version"] = CONFIG_VERSION;

            JsonArray pots = doc.createNestedArray("pots");
            for (uint8_t i = 0; i < configManager.getNumPots(); ++i) {
                JsonObject pot = pots.createNestedObject();
                pot["index"] = i;
                pot["channel"] = configManager.getPotChannel(i);
                pot["cc"] = configManager.getPotCCNumber(i);
            }

            JsonArray slots = doc.createNestedArray("slots");
            const auto &slotDefs = configManager.getSlots();
            for (uint8_t i = 0; i < NUM_SLOTS; ++i) {
                const MIDISlot &slot = slotDefs[i];
                JsonObject slotObj = slots.createNestedObject();
                slotObj["index"] = i;
                slotObj["type"] = static_cast<uint8_t>(slot.type);
                slotObj["type_name"] = midiMessageTypeName(slot.type);
                slotObj["channel"] = slot.midiChannel;
                slotObj["data1"] = slot.data1;
                slotObj["ef_index"] = slot.ef.followerIndex;
                JsonObject ef = slotObj.createNestedObject("ef");
                ef["index"] = slot.ef.followerIndex;
                ef["filter_index"] = static_cast<uint8_t>(slot.efSettings.filterType);
                ef["filter_name"] = efFilterLabel(slot.efSettings.filterType);
                ef["frequency"] = slot.efSettings.frequency;
                ef["q"] = slot.efSettings.q;
                ef["oversample"] = slot.efSettings.oversample;
                ef["smoothing"] = slot.efSettings.smoothing;
                ef["baseline"] = slot.efSettings.baseline;
                ef["gain"] = slot.efSettings.gain;
                // Extended EF mode + auto-calibration payload for the frontend.
                ef["mode"] = slot.efSettings.efMode;
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
                slotObj["active"] = slot.active;
                slotObj["arp_note"] = slot.arpNote;
                slotObj["sysexTemplate"] = formatSysExTemplate(slot);
                SlotEnvelopePayload payload = configManager.getSlotEnvelopePayload(i);
                JsonObject efPayload = slotObj.createNestedObject("ef_payload");
                efPayload["type"] = payload.filterType;
                efPayload["type_name"] = envelopeFilterName(
                    static_cast<EnvelopeFollower::FilterType>(payload.filterType));
                efPayload["freq"] = payload.frequency;
                efPayload["q"] = payload.q;
                SlotARGConfig arg = sanitizeSlotArg(slot.arg);
                JsonObject argObj = slotObj.createNestedObject("arg");
                argObj["enabled"] = arg.enabled != 0;
                argObj["method"] = static_cast<uint8_t>(arg.method);
                argObj["method_name"] = argMethodName(static_cast<uint8_t>(arg.method));
                argObj["sourceA"] = arg.sourceA;
                argObj["sourceB"] = arg.sourceB;
            }

            JsonObject env = doc.createNestedObject("envelopes");
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

            float freq = 0.0f;
            float q = 0.0f;
            EEPROM.get(EEPROM_FILTER_FREQ, freq);
            EEPROM.get(EEPROM_FILTER_Q, q);
            JsonObject filter = env.createNestedObject("filter");
            filter["frequency"] = freq;
            filter["q"] = q;

            JsonObject led = doc.createNestedObject("led");
            led["brightness"] = ledManager.getBrightness();
            CRGB color = ledManager.getColor();
            JsonObject colorObj = led.createNestedObject("rgb");
            colorObj["r"] = color.r;
            colorObj["g"] = color.g;
            colorObj["b"] = color.b;
            char hex[8];
            snprintf(hex, sizeof(hex), "#%02X%02X%02X", color.r, color.g, color.b);
            led["hex"] = hex;

            String payload;
            serializeJson(doc, payload);
            LOG_PRINTLN(payload);

        } else if (command.startsWith("GET_PROFILE")) {
            int comma = command.indexOf(',');
            uint8_t id = g_activeProfile;
            if (comma >= 0) {
                id = static_cast<uint8_t>(command.substring(comma + 1).toInt());
            }
            if (id >= NUM_PROFILES) {
                LOG_PRINTLN("ERR");
                continue;
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

            String payload;
            serializeJson(doc, payload);
            LOG_PRINTLN(payload);

        } else if (command.startsWith("SET_PROFILE")) {
            int firstComma = command.indexOf(',');
            int secondComma = command.indexOf(',', firstComma + 1);
            if (firstComma < 0 || secondComma < 0) {
                LOG_PRINTLN("ERR");
                continue;
            }
            uint8_t id =
                static_cast<uint8_t>(command.substring(firstComma + 1, secondComma).toInt());
            if (id >= NUM_PROFILES) {
                LOG_PRINTLN("ERR");
                continue;
            }
            String payload = command.substring(secondComma + 1);
            payload.trim();
            if (payload.length() == 0) {
                LOG_PRINTLN("ERR");
                continue;
            }
            StaticJsonDocument<12288> doc;
            DeserializationError err = deserializeJson(doc, payload);
            if (err) {
                LOG_PRINTLN("ERR");
                continue;
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
                        profile.lfos[index].syncRatio =
                            static_cast<uint8_t>(lfo["sync_ratio"].as<int>());
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
                            static_cast<uint8_t>(slot["channel"].as<int>());
                    }
                    if (slot.containsKey("ef")) {
                        JsonObject ef = slot["ef"].as<JsonObject>();
                        parseProfileEf(ef, profile.slots[index].ef);
                    }
                }
            }

            if (!configManager.saveProfileSettings(id, profile)) {
                LOG_PRINTLN("ERR");
                continue;
            }
            if (id == g_activeProfile) {
                ProfileData stored{};
                if (configManager.loadProfileSettings(id, stored)) {
                    applyProfileSnapshot(stored, true);
                }
            }
            LOG_PRINTLN("OK");

        } else if (command.startsWith("SET_POT")) {
            // Parse "SET_POT" command
            int firstComma = command.indexOf(',');
            int lastComma = command.lastIndexOf(',');

            if (firstComma == -1 || lastComma == -1 || firstComma == lastComma) {
                LOG_PRINTLN("Error: Malformed SET_POT command");
                continue; // Skip invalid command
            }

            int potIndex = command.substring(8, firstComma).toInt();
            int channel = command.substring(firstComma + 1, lastComma).toInt();
            int ccNumber = command.substring(lastComma + 1).toInt();

            if (potIndex >= 0 && potIndex < NUM_POTS && channel >= 1 && channel <= 16 &&
                ccNumber >= 0 && ccNumber <= 127) {
                configManager.setPotChannel(potIndex, channel);
                configManager.setPotCCNumber(potIndex, ccNumber);
                potentiometerManager.setChannel(potIndex, channel);
                potentiometerManager.setCCNumber(potIndex, ccNumber);
                if (static_cast<size_t>(potIndex) < potChannels.size()) {
                    potChannels[potIndex] = channel;
                }
                configManager.saveConfiguration();
                LOG_PRINTLN("Pot configuration updated!");
            } else {
                LOG_PRINTLN("Error: Invalid values for SET_POT");
            }

        } else if (command.startsWith("SET_ALL")) {
            String chunk = command.substring(8);
            chunk.trim();
            if (chunk.length() == 0) {
                continue;
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
                continue;
            }

            static StaticJsonDocument<Utility::kMaxBulkConfigSize> doc;
            // Persist the 32 KB document between uploads so we don't hammer the stack.
            doc.clear();
            DeserializationError err = deserializeJson(doc, bulkConfigAssembler.payload());
            if (err == DeserializationError::IncompleteInput) {
                continue;
            }
            if (err) {
                emitBulkError("parse", err.c_str(), bulkConfigAssembler.sequenceHint());
                bulkConfigAssembler.reset();
                continue;
            }

            uint32_t seq = doc["seq"].as<uint32_t>();
            if (seq == 0) {
                seq = bulkConfigAssembler.sequenceHint();
            }
            const char *checksum = doc["checksum"] | nullptr;
            if (!checksum || checksum[0] == '\0') {
                emitBulkError("checksum", "missing checksum", seq);
                bulkConfigAssembler.reset();
                continue;
            }

            if (seq == 0) {
                seq = lastAckSequence + 1;
            }

            if (seq == lastAckSequence && lastAckChecksum == checksum) {
                LOG_PRINTLN(Utility::formatAck(checksum, seq));
                bulkConfigAssembler.reset();
                continue;
            }

            JsonObject configObj = doc["config"].as<JsonObject>();
            if (!applyConfigObject(configObj, seq)) {
                bulkConfigAssembler.reset();
                continue;
            }

            lastAckSequence = seq;
            lastAckChecksum = checksum;
            LOG_PRINTLN(Utility::formatAck(checksum, seq));
            bulkConfigAssembler.reset();

        } else if (command.startsWith("GET_ALL")) {
#ifdef SERIAL_LOGGING
            // Send all pot settings
            LOG_PRINT("POTS:");
            for (int i = 0; i < NUM_POTS; i++) {
                int envelopeValue = -1;
                auto it = potToEnvelopeMap.find(i);
                if (it != potToEnvelopeMap.end()) {
                    envelopeValue = it->second.followerIndex;
                }
                LOG_PRINT(configManager.getPotCCNumber(i));
                LOG_PRINT(",");
                LOG_PRINT(configManager.getPotChannel(i));
                LOG_PRINT(",");
                LOG_PRINT(envelopeValue);
                LOG_PRINT(";");
            }

            // Send LED settings
            CRGB ledColor = ledManager.getColor();
            LOG_PRINT(" LED:");
            LOG_PRINT(ledManager.getBrightness());
            LOG_PRINT(",");
            LOG_PRINT(ledColor.r);
            LOG_PRINT(",");
            LOG_PRINT(ledColor.g);
            LOG_PRINT(",");
            LOG_PRINTLN(ledColor.b);
#endif
        } else if (command == "GET_LED") {
#ifdef SERIAL_LOGGING
            CRGB c = ledManager.getColor();
            LOG_PRINT(ledManager.getBrightness());
            LOG_PRINT(",");
            LOG_PRINT(c.r);
            LOG_PRINT(",");
            LOG_PRINT(c.g);
            LOG_PRINT(",");
            LOG_PRINTLN(c.b);
#endif
        } else if (command.startsWith("SET_LED")) {
            int first = command.indexOf(',');
            int second = command.indexOf(',', first + 1);
            int third = command.indexOf(',', second + 1);
            if (first == -1 || second == -1 || third == -1) {
                LOG_PRINTLN("ERR");
            } else {
                int brightness = command.substring(8, first).toInt();
                int r = command.substring(first + 1, second).toInt();
                int g = command.substring(second + 1, third).toInt();
                int b = command.substring(third + 1).toInt();
                if (brightness >= 0 && brightness <= 255 && r >= 0 && r <= 255 && g >= 0 &&
                    g <= 255 && b >= 0 && b <= 255) {
                    CRGB color(r, g, b);
                    ledManager.setBrightness(brightness);
                    ledManager.setColor(color);
                    configManager.saveLEDSettings(brightness, color);
                    LOG_PRINTLN("OK");
                } else {
                    LOG_PRINTLN("ERR");
                }
            }
        } else if (command == "GET_ARGMETHOD") {
            LOG_PRINTLN(configManager.getARGMethod());
        } else if (command.startsWith("SET_ARGMETHOD")) {
            // SET_ARGMETHOD <method>
            // method: 0-13 mapping to ARGMethod values; see docs for math notes.
            // Side effects: updates every slot's ARG method and burns the
            // legacy default into EEPROM via ConfigManager.
            int method = command.substring(14).toInt();
            if (method >= 0 && method <= static_cast<int>(ARGMethod::XORR)) {
                for (uint8_t slotIndex = 0; slotIndex < NUM_SLOTS; ++slotIndex) {
                    MIDISlot &slot = configManager.getSlot(slotIndex);
                    slot.arg.method = static_cast<ARGMethod>(method);
                    configManager.saveSlot(slotIndex, slot);
                }
                configManager.setARGMethod(static_cast<uint8_t>(method));
                LOG_PRINTLN("OK");
            } else {
                // out-of-range method? we spit ERR
                LOG_PRINTLN("ERR");
            }
        } else if (command.startsWith("GET_EF")) {
            // GET_EF <slot>
            // slot: 0..NUM_POTS-1. Reports which envelope (or -1) owns it.
            // See firmware/README.md#L730-L744 or docs/WebSerial.md#L60-L72 for
            // the whole WebSerial spiel.
            int potIndex = command.substring(7).toInt();
            if (potIndex >= 0 && potIndex < NUM_POTS) {
                int env = -1;
                auto it = potToEnvelopeMap.find(potIndex);
                if (it != potToEnvelopeMap.end()) {
                    env = it->second.followerIndex;
                }
#ifdef SERIAL_LOGGING
                LOG_PRINTLN(env);
#else
                (void)env;
#endif
            } else {
                // bogus slot index
                LOG_PRINTLN("ERR");
            }
        } else if (command.startsWith("SET_EF")) {
            // SET_EF <slot,env>
            // slot: 0..NUM_POTS-1, env: 0..envelopeFollowers.size()-1
            // Side effects: maps slot to follower, flips it active, and
            // saves mapping/baseline to EEPROM. See firmware/README.md#L730-L744
            // and firmware/include/EnvelopeFollower/README.md for EF guts.
            int comma = command.indexOf(',');
            if (comma == -1) {
                LOG_PRINTLN("ERR");
            } else {
                int potIndex = command.substring(7, comma).toInt();
                int envIndex = command.substring(comma + 1).toInt();
                if (potIndex >= 0 && potIndex < NUM_POTS && envIndex >= 0 &&
                    envIndex < (int)envelopeFollowers.size()) {
                    MIDISlot &slot = configManager.getSlot(static_cast<uint8_t>(potIndex));
                    slot.setEnvelopeFollowerIndex(static_cast<int8_t>(envIndex));
                    potToEnvelopeMap[potIndex] = slot.efSettings;
                    envelopeFollowers[envIndex].toggleActive(true);
                    applyEfSettingsToFollower(envelopeFollowers[envIndex], slot.efSettings,
                                              static_cast<uint8_t>(envIndex));
                    configManager.saveEnvelopeSettings(potToEnvelopeMap, envelopeFollowers);
                    refreshEfVoicesFromConfig();
                    LOG_PRINTLN("OK");
                } else {
                    // numbers don't line up? it's an ERR
                    LOG_PRINTLN("ERR");
                }
            }
        } else if (configManager.handleCommand(command)) {
            // handled inside ConfigManager
        } else {
            LOG_PRINTLN("Unknown command: " + command);
        }
    }
}

void processLFOs() {
    // Update the LFO engine and mirror its bus values into globals.
    lfoManager.update(now());
    const LFOBus &bus = lfoManager.bus();
    g_lfoEfGainTrim = bus.efGainTrim;
    g_lfoArpSwing = bus.arpSwing;
    g_lfoLedBrightness = bus.ledBrightness;
    g_lfoValues[0] = lfoManager.normalizedValue(0);
    g_lfoValues[1] = lfoManager.normalizedValue(1);

    // Apply LFO-driven brightness modulation on top of the base LED level.
    float brightnessScale = 1.0f + g_lfoLedBrightness;
    brightnessScale = constrain(brightnessScale, 0.0f, 2.0f);
    ledManager.setBrightnessModulator(brightnessScale);
}

void configureLFOs() {
    // Reset routes and seed default LFO state (depth 0 so no modulation yet).
    lfoManager.clearRoutes();

    LFO &lfo1 = lfoManager.lfo(0);
    lfo1.setShape(LFOShape::Sine);
    lfo1.setFrequencyHz(1.0f);
    lfo1.setDepth(0.0f); // idle until configured
    lfo1.setBipolar(false);
    lfo1.setSyncEnabled(false);
    lfo1.setSyncRatio(LFOSyncRatio::Div1);

    LFO &lfo2 = lfoManager.lfo(1);
    lfo2.setShape(LFOShape::Triangle);
    lfo2.setFrequencyHz(0.5f);
    lfo2.setDepth(0.0f); // idle until configured
    lfo2.setBipolar(true);
    lfo2.setSyncEnabled(false);
    lfo2.setSyncRatio(LFOSyncRatio::Div1);

    // Default internal routes: LFO1 -> LEDs + arp swing, LFO2 -> EF gain trim.
    lfoManager.addInternalRoute(0, LFOInternalTarget::LedBrightness, 1.0f);
    lfoManager.addInternalRoute(0, LFOInternalTarget::ArpSwing, 1.0f);
    lfoManager.addInternalRoute(1, LFOInternalTarget::EfGainTrim, 1.0f);
}

void processEnvelopes() {
    static std::array<uint8_t, NUM_POTS> lastEnvelopeMidiValues;
    static bool envelopeMidiInitialized = false;
    if (!envelopeMidiInitialized) {
        lastEnvelopeMidiValues.fill(0xFF); // 0xFF sentinel guarantees the first send happens
        envelopeMidiInitialized = true;
    }

    std::array<int, NUM_ENVELOPES> rawFollowerLevels{};
    std::array<bool, NUM_ENVELOPES> followerReady{};
    // LFO gain trim scales all active envelope followers in this frame.
    float gainTrim = 1.0f + g_lfoEfGainTrim;
    gainTrim = constrain(gainTrim, 0.0f, 2.0f);
    for (size_t idx = 0; idx < envelopeFollowers.size(); ++idx) {
        EnvelopeFollower &follower = envelopeFollowers[idx];
        if (!follower.getActiveState()) {
            followerReady[idx] = false;
            continue;
        }
        follower.setExternalGainTrim(gainTrim);
        follower.update();
        int rawLevel = follower.getEnvelopeLevel();
        rawFollowerLevels[idx] = rawLevel;
        followerReady[idx] = true;
        ledManager.setEnvelopeLevel(static_cast<uint8_t>(idx), constrain(rawLevel, 0, 127));
    }

    // Stroll through the pot→envelope map; every pair says which envelope
    // rides shotgun with which physical pot.
    for (const auto &entry : potToEnvelopeMap) {
        const int potIndex = entry.first;
        const MIDISlot::EfSettings &settings = entry.second;
        const int envelopeIndex = settings.followerIndex;
        if (potIndex < 0 || potIndex >= NUM_POTS)
            continue; // Someone scribbled junk into the routing table; bail fast
        if (envelopeIndex >= 0 && envelopeIndex < static_cast<int>(envelopeFollowers.size())) {
            int currentPotReading = potentiometerManager.getLastValue(potIndex);
            if (currentPotReading < 0)
                continue; // No baseline yet; wait for the pot scanner to catch up

            uint8_t baselineMidi = Utility::mapToMidiValue(currentPotReading);

            EnvelopeFollower *envelope = &envelopeFollowers[envelopeIndex];
            bool envelopeActive = envelope->getActiveState();

            if (!envelopeActive) {
                // Keep the cache glued to the pot so the first pass after wake-up
                // rides on the current baseline instead of a stale MIDI value.
                lastEnvelopeMidiValues[potIndex] = baselineMidi;
                continue;
            }

            if (!followerReady[envelopeIndex]) {
                lastEnvelopeMidiValues[potIndex] = baselineMidi;
                continue;
            }

            const MIDISlot &slot = configManager.getSlot(potIndex);
            EfVoice &voice = efVoices[potIndex];
            voice.assignFollower(envelopeIndex);
            voice.syncSettings(slot.efSettings);
            voice.render(rawFollowerLevels[envelopeIndex]);

            if (potIndex >= NUM_SLOTS)
                continue;
            uint8_t envelopeContribution = computeSlotArgLevel(slot, envelopeFollowers);
            int modulatedInt = static_cast<int>(baselineMidi) + envelopeContribution;
            uint8_t modulatedValue = static_cast<uint8_t>(constrain(modulatedInt, 0, 127));

            bool valueChanged = modulatedValue != lastEnvelopeMidiValues[potIndex];
            if (valueChanged) {
                // Only shout over MIDI when the value actually moved.
                uint8_t ccNumber = potentiometerManager.getCCNumber(potIndex);
                uint8_t channel = potentiometerManager.getChannel(potIndex);
                midiHandler.sendControlChange(ccNumber, modulatedValue, channel);
                lastEnvelopeMidiValues[potIndex] = modulatedValue;
            }

            // Mirror the post-modulation value on the pot LED so the panel reflects the swing.
            ledManager.setPotValue(potIndex, modulatedValue);
        }
    }

    // After the dust settles, mirror the active pot's MIDI-scaled value on
    // every indicator LED so the panel shows exactly what that knob is yelling.
    if (buttonContext.activePot < NUM_POTS) {
        int activePotReading = potentiometerManager.getLastValue(buttonContext.activePot);
        if (activePotReading >= 0) {
            uint8_t potMidiValue = Utility::mapToMidiValue(activePotReading);
            for (uint8_t i = 0; i < POT_LED_COUNT; ++i) {
                ledManager.setPotIndicator(i, potMidiValue);
            }
        }
    }
}

// Kick out MIDI clock pulses if the outside world bails on us.
void processInternalClock() {
    static unsigned long lastInternalTick = 0;
    if (g_tappedBPM <= 0.0f)
        return; // No tempo tapped, nothing to do

    float msPerTick = 60000.0f / (g_tappedBPM * 24.0f); // 24 PPQN
    unsigned long now = ::now();
    if (now - lastInternalTick >= msPerTick) {
        lastInternalTick = now;
        lastClockTime = now;

        // Chuck out a clock tick if we're allowed to shout. The MIDI handler will
        // mirror it out and bump the shared counter so processMIDI() can advance
        // beats and refresh the display exactly once per pulse.
        midiHandler.generateClockTick();
    }
}

// Measure how often the main loop cycles each second—our quick-and-dirty load meter.
// On a healthy Teensy 4.0 we usually see about a kilospin per second; if it tanks,
// you're choking the core.
void monitorSystemLoad() {
    static unsigned long lastMonitorTime = 0;
    static unsigned long taskCounter = 0; // main loop iterations
    static unsigned long maxLoopDuration = 0;
    static unsigned long lastLoopStart = micros();

    unsigned long currentMicros = micros();
    unsigned long loopDuration = currentMicros - lastLoopStart;
    lastLoopStart = currentMicros;

    g_systemDiagnostics.lastLoopMicros = loopDuration;
    if (loopDuration > maxLoopDuration) {
        maxLoopDuration = loopDuration;
    }
    if (loopDuration > 1000UL) {
        ++g_systemDiagnostics.loopOverrunCount;
        LOG_PRINTF("{\"diagnostic\":\"loop_overrun\",\"duration_us\":%lu}\n",
                   static_cast<unsigned long>(loopDuration));
        requestStatusLEDPulse();
    }

    taskCounter++; // count another lap around the loop
    unsigned long currentMillis = now();
    if (currentMillis - lastMonitorTime >= 1000UL) {        // Log every second
        LOG_PRINTF("Tasks per second: %lu\n", taskCounter); // ~1000 on a chill rig
        g_systemDiagnostics.maxLoopMicros = maxLoopDuration;
        maxLoopDuration = 0;
        taskCounter = 0;
        lastMonitorTime = currentMillis;
    }

    checkDiagnosticsForAlerts();
    serviceStatusLEDPulse();
}

void updateFilterTuning(ButtonManagerContext &context) {
    if (g_jitterTuningActive) {
        return;
    }
    // 1. Read raw ADC from freq pot
    int rawFreq = buttonManager.getControlPotValue(1); // MUXC channel 13
    // 2. Read raw ADC from Q pot
    int rawQ = buttonManager.getControlPotValue(2); // MUXC channel 14

    // 3. Map rawFreq => 20..5000 Hz (pick a range that feels good)
    float freq = map(rawFreq, 0, 1023, 20, 5000);

    // 4. Map rawQ => 0.5..4.0 (a typical resonance range)
    //    - For instance, map from 0..1023 => 50..400, then /100
    float q = map(rawQ, 0, 1023, 50, 400) / 100.0f; // => 0.50..4.00

    // 5. Which EF are we tuning?
    //    We'll tune the EF assigned to the “activePot” in the context
    auto it = context.potToEnvelopeMap.find(context.activePot);
    if (it == context.potToEnvelopeMap.end()) {
        // If no EF assigned to active pot, do nothing
        return;
    }
    MIDISlot::EfSettings &settings = it->second;
    int efIndex = settings.followerIndex; // e.g. 0..5 if you have 6 EFs total
    if (efIndex < 0 || efIndex >= static_cast<int>(context.envelopes.size())) {
        return;
    }

    // 6. Actually set that EF’s filter freq/Q
    //    BUT remember, it only affects EFs whose filterType is
    //    LOWPASS, HIGHPASS, or BANDPASS.
    SlotEnvelopePayload tailPayload{};
    tailPayload.filterType = static_cast<uint8_t>(context.envelopes[efIndex].getFilterType());
    tailPayload.frequency = freq;
    tailPayload.q = q;
    SlotEnvelopePayload sanitizedTail = configManager.persistFilterTail(tailPayload);
    freq = sanitizedTail.frequency;
    q = sanitizedTail.q;
    context.envelopes[efIndex].configureFilter(freq, q);
    if (context.activePot < NUM_SLOTS) {
        SlotEnvelopePayload payload =
            configManager.getSlotEnvelopePayload(static_cast<uint8_t>(context.activePot));
        payload.filterType = static_cast<uint8_t>(context.envelopes[efIndex].getFilterType());
        payload.frequency = freq;
        payload.q = q;
        configManager.setSlotEnvelopePayload(static_cast<uint8_t>(context.activePot), payload);
        WebSerial::sendSlotPatch(configManager, static_cast<uint8_t>(context.activePot));
    }
    // Provide labels for the on-screen filter tuning display
    displayManager.showFilterTuning("Freq", freq, "Q", q);

    // Optionally display or debug-print
    // LOG_PRINTF("EF %d => freq=%.1f Q=%.2f\n", efIndex, freq, q);
}

void updateArpTuning() {
    if (g_jitterTuningActive) {
        return;
    }
    if (!arpeggiator.isActive())
        return;

    int raw1 = buttonManager.getControlPotValue(1);
    int raw2 = buttonManager.getControlPotValue(2);

    if (g_arpEditActive) {
        // Arp edit mode maps control pots to gate length and octave range.
        uint8_t gatePercent = map(raw1, 0, 1023, 5, 100);
        uint8_t octaveRange = map(raw2, 0, 1023, 0, 3);
        arpeggiator.setGatePercent(gatePercent);
        arpeggiator.setOctaveRange(octaveRange);
        char line2[24];
        char line3[24];
        snprintf(line2, sizeof(line2), "Gate %u%%", gatePercent);
        snprintf(line3, sizeof(line3), "Oct +%u", octaveRange);
        displayManager.showText("Arp Edit", line2, line3);
        return;
    }

    // Knob #1 owns the step length. Its raw 10‑bit reading gets linearly remapped
    // to MIDI clock ticks so the riff stays welded to the global tempo:
    //   0     -> 1 tick   (every pulse)
    //   1023  -> MAX_LENGTH ticks (a whole quarter note at 24 PPQN)
    // Future hackers: tweak Arpeggiator::MAX_LENGTH if you want longer gaps.
    uint8_t lengthTicks = map(raw1, 0, 1023, 1, Arpeggiator::MAX_LENGTH);
    int shapeIdx = map(raw2, 0, 1023, 0, 5);
    static const char *names[] = {"UP", "DOWN", "UPDN", "RAND", "DRUNK", "EUCL"};
    Arpeggiator::Shape shapes[] = {Arpeggiator::UP,     Arpeggiator::DOWN,  Arpeggiator::UPDOWN,
                                   Arpeggiator::RANDOM, Arpeggiator::DRUNK, Arpeggiator::EUCLIDEAN};

    // Update the arpeggiator with the latest UI control values.
    arpeggiator.setLength(lengthTicks);
    arpeggiator.setShape(shapes[shapeIdx]);

    displayManager.showArpSettings(lengthTicks, names[shapeIdx]);
}

void updateNoteDynamics() {
    if (g_jitterTuningActive) {
        return;
    }
    if (arpeggiator.isActive())
        return;

    int rawShift = buttonManager.getControlPotValue(1);
    int rawProb = buttonManager.getControlPotValue(2);

    velocityShift = map(rawShift, 0, 1023, -64, 63);
    changeProbability = static_cast<uint8_t>(map(rawProb, 0, 1023, 0, 100));

    String line2 = String("Vel ") + String(velocityShift);
    String line3 = String("Prob ") + String(changeProbability) + "%";
    displayManager.showText("Note Dyn", line2.c_str(), line3.c_str());
}

void streamWebSerialState() {
    if (!webSerialStreaming)
        return;
    const SystemDiagnostics diagSnapshot = captureDiagnosticsSnapshot();
    WebSerial::sendStateSnapshot(potentiometerManager, envelopeFollowers, configManager,
                                 buttonContext.activePot, diagSnapshot);
}

void setup() {
    // — Serial & Config —
    Serial.begin(SERIAL_BAUD);
    Serial.printf("MN42 FW %s %s\n", FW_VERSION_STR, GIT_SHA_STR);
    g_resetCause = SRC_SRSR;
    EEPROM.get(EEPROM_BROWNOUT_COUNT, g_brownoutCount);
    if (g_brownoutCount == 0xFFFF) {
        g_brownoutCount = 0;
        EEPROM.put(EEPROM_BROWNOUT_COUNT, g_brownoutCount);
    }
    if (g_resetCause & 0x40) {
        g_brownoutCount++;
        EEPROM.put(EEPROM_BROWNOUT_COUNT, g_brownoutCount);
    }
    Serial.printf("MN42 FW %s schema %04X UID %08lX%08lX%08lX%08lX\n", FW_VERSION_STR,
                  CONFIG_VERSION, HW_OCOTP_CFG0, HW_OCOTP_CFG1, HW_OCOTP_CFG2, HW_OCOTP_CFG3);
    Serial.printf("Reset 0x%08lX Brownouts %u\n", g_resetCause, g_brownoutCount);

    // Configure status LED
    pinMode(hwConfig.statusLedPin, OUTPUT);
    digitalWrite(hwConfig.statusLedPin, LOW);

    // Measure VREF for baseline calibration
    pinMode(VREF_ADC_PIN, INPUT);
    g_vref = Utility::readVrefADC(VREF_ADC_PIN);

    // Load per-slot EEPROM into RAM, and pot→channel data into potChannels[]
    configManager.begin(potChannels);
    configManager.loadMIDISlots(&configManager.getSlot(0), NUM_SLOTS);
    bool baselinesLoaded = configManager.loadEnvelopeSettings(potToEnvelopeMap, envelopeFollowers);
    refreshEfVoicesFromConfig();
    for (size_t i = 0; i < envelopeFollowers.size(); ++i) {
        if (i < efBaseGains.size()) {
            efBaseGains[i] = envelopeFollowers[i].getGain();
        }
    }

    // — MIDI Handler —
    midiHandler.begin();
    midiHandler.setDiagnostics(&g_systemDiagnostics);
    midiHandler.setDisplayManager(&displayManager);
    seedbox::interop::mn42::SeedBoxLink::instance().begin(&midiHandler);
    // LFOs need MIDI tick access for clock sync routing.
    lfoManager.attachMIDI(&midiHandler);
    configureLFOs();

    // — Profiles —
    // Restore the active profile index and load its pot map + extended payload.
    g_activeProfile = configManager.getActiveProfile();
    if (g_activeProfile >= NUM_PROFILES) {
        g_activeProfile = 0;
        configManager.setActiveProfile(g_activeProfile);
    }
    configManager.loadProfile(g_activeProfile);
    potChannels.clear();
    for (uint8_t i = 0; i < configManager.getNumPots(); ++i) {
        potChannels.push_back(configManager.getPotChannel(i));
    }
    ProfileData storedProfile{};
    if (configManager.loadProfileSettings(g_activeProfile, storedProfile)) {
        applyProfileSnapshot(storedProfile, true);
    }

    // — Pot → MIDI routing callback —
    potentiometerManager.setMidiCallback(
        [&](uint8_t /*ccNumber*/, uint8_t value, uint16_t rawValue, uint8_t potIdx) {
            auto &slot = configManager.getSlot(potIdx);
            if (!slot.active)
                return;

            switch (slot.type) {
            case MIDIMessageType::CC:
                midiHandler.sendControlChange(slot.data1, value, slot.midiChannel);
                break;

            case MIDIMessageType::Note: {
                uint8_t note = Utility::mapToMidiValue(rawValue) % 128;
                slot.arpNote = note; // stash for the arpeggiator
                uint8_t velo = 125;
                if (potIdx < efVoices.size() && efVoices[potIdx].hasRendered()) {
                    velo = efVoices[potIdx].latestLevel();
                } else if (slot.ef.followerIndex >= 0 &&
                           static_cast<size_t>(slot.ef.followerIndex) < envelopeFollowers.size()) {
                    velo = static_cast<uint8_t>(
                        constrain(envelopeFollowers[static_cast<size_t>(slot.ef.followerIndex)]
                                      .getEnvelopeLevel(),
                                  0, 127));
                }
                int shifted = velo + velocityShift;
                if (shifted < 0)
                    shifted = 0;
                if (shifted > 127)
                    shifted = 127;
                if (random(100U) >= changeProbability)
                    break;
                midiHandler.sendNoteOn(note, shifted, slot.midiChannel);
                // schedule Note-Off in 100 ms
                Utility::schedulerHigh.addTask(
                    [=]() { midiHandler.sendNoteOff(note, 0, slot.midiChannel); }, 100);
                break;
            }
            case MIDIMessageType::PitchBend: {
                int16_t bend = map(static_cast<int>(rawValue), 0, 1023, -8192, 8191);
                midiHandler.sendPitchBend(bend, slot.midiChannel);
                break;
            }

            case MIDIMessageType::ProgramChange:
                midiHandler.sendProgramChange(slot.data1, slot.midiChannel);
                break;

            case MIDIMessageType::Aftertouch: {
                uint8_t pres = Utility::mapToMidiValue(rawValue);
                midiHandler.sendAftertouch(pres, slot.midiChannel);
                break;
            }

            case MIDIMessageType::ModWheel: {
                uint8_t mod = Utility::mapToMidiValue(rawValue);
                midiHandler.sendModWheel(mod, slot.midiChannel);
                break;
            }

            case MIDIMessageType::NRPN: {
                uint16_t param = static_cast<uint16_t>(slot.data1) << 7; // LSB zeroed
                uint16_t val = static_cast<uint16_t>(Utility::mapToMidiValue(rawValue)) << 7;
                midiHandler.sendNRPN(param, val, slot.midiChannel);
                break;
            }

            case MIDIMessageType::RPN: {
                uint16_t param = static_cast<uint16_t>(slot.data1) << 7; // LSB zeroed
                uint16_t val = static_cast<uint16_t>(Utility::mapToMidiValue(rawValue)) << 7;
                midiHandler.sendRPN(param, val, slot.midiChannel);
                break;
            }

            case MIDIMessageType::SysEx: {
                std::array<uint8_t, SysExTemplate::kMaxLength> msg{};
                uint8_t length = buildSysExPayload(slot, rawValue, msg.data(), msg.size());
                if (length > 0) {
                    midiHandler.sendSysEx(msg.data(), length);
                }
                break;
            }

            default:
                break;
            }
        });

    // — LEDs & Display —
    ledManager.begin();
    uint8_t ledB;
    CRGB ledC;
    configManager.loadLEDSettings(ledB, ledC);
    ledManager.setBrightness(ledB);
    ledManager.setColor(ledC);

    displayManager.begin();
    displayManager.showText("Initializing...");

    // — EEPROM & Mux init —
    potentiometerManager.loadFromEEPROM();

    // — Timer (1 ms base for MIDI & internal clock) —
    Timer1.initialize(1000);
    Timer1.attachInterrupt(processMIDI);

    // — Filter hardware —
    filter.configure(BiquadFilter::LOWPASS, 1000, 44100);

    // — Envelope followers —
    for (auto &ef : envelopeFollowers) {
        ef.toggleActive(true);
        if (!baselinesLoaded) {
            ef.calibrate();
        }
    }
    float sf, sq;
    EEPROM.get(EEPROM_FILTER_FREQ, sf);
    EEPROM.get(EEPROM_FILTER_Q, sq);
    sf = constrain(sf, 20.0f, 5000.0f);
    sq = constrain(sq, 0.5f, 4.0f);
    for (auto &ef : envelopeFollowers)
        ef.configureFilter(sf, sq);

    // — Slot sanity check (channel & CC) —
    for (uint8_t i = 0; i < NUM_SLOTS; i++) {
        if (potentiometerManager.getChannel(i) == 0)
            potentiometerManager.setChannel(i, 1);
        if (potentiometerManager.getCCNumber(i) > 127)
            potentiometerManager.setCCNumber(i, i % 128);
    }

    // — Load or reset full config —
    if (!configManager.loadConfiguration(potChannels)) {
        LOG_PRINTLN("EEPROM corrupted → resetting.");
        potentiometerManager.resetEEPROM();
    }

    // — Buttons & splash —
    buttonManager.initButtons();
    delay(1000);
    displayManager.clear();
    displayManager.showText("MOAR");
    ledManager.blinkStatusLED(2, 100);

    displayManager.runStartupAnimation();

    // — Scheduler tasks —
    // Three cooperative schedulers slice time so nothing blocks:
    // High-priority (1 ms):
    Utility::schedulerHigh.addTask(processMIDI, hwConfig.midiTaskInterval);
    // Keep LFO updates on a tight 1 kHz cadence for smooth modulation.
    Utility::schedulerHigh.addTask(processLFOs, 1, true);
    Utility::schedulerHigh.addTask(
        []() {
            if (now() - lastClockTime > CLOCK_TIMEOUT_MS)
                processInternalClock();
        },
        hwConfig.midiTaskInterval);
    Utility::schedulerHigh.addTask(
        []() { arpeggiator.update(midiHandler, configManager, potentiometerManager); },
        hwConfig.midiTaskInterval);

    // Mid-priority (~5 ms):
    Utility::schedulerMid.addTask(processSerial, hwConfig.serialTaskInterval);
    Utility::schedulerMid.addTask(processEnvelopes, hwConfig.envelopeTaskInterval);

    // Low-priority (~50 ms):
    Utility::schedulerLow.addTask(
        []() {
            ledManager.update();
            updateFilterTuning(buttonContext);
            updateArpTuning();
            updateNoteDynamics();
        },
        hwConfig.ledTaskInterval);

    Utility::schedulerLow.addTask(
        []() {
            if (diagnosticMode) {
                displayManager.beginDraw();
                const SystemDiagnostics diagSnapshot = captureDiagnosticsSnapshot();
                displayManager.showDiagnostic(diagnosticPage, buttonManager, buttonContext,
                                              midiHandler, diagSnapshot);
                displayManager.endDraw();
            } else if (!displayManager.shouldRunScreensaver()) {
                displayManager.beginDraw();
                displayManager.updateFromContext(buttonContext);
                auto it = potToEnvelopeMap.find(buttonContext.activePot);
                if (it != potToEnvelopeMap.end()) {
                    displayManager.showEnvelopeLevel(
                        efVoices[buttonContext.activePot].latestLevel());
                    int follower = it->second.followerIndex;
                    if (follower >= 0 && follower < static_cast<int>(envelopeFollowers.size())) {
                        displayManager.showEnvelopeLevel(
                            envelopeFollowers[follower].getEnvelopeLevel());
                    }
                }
                displayManager.highlightActivePot(buttonContext.activePot);
                displayManager.highlightActiveMode(envelopeMode);
                displayManager.endDraw();
            } else {
                displayManager.runIdleScreensaver();
            }
        },
        50);

    Utility::schedulerLow.addTask(
        []() { seedbox::interop::mn42::SeedBoxLink::instance().update(); }, 500);

    // WebSerial telemetry every ~100 ms once the browser says hello
    Utility::schedulerLow.addTask(streamWebSerialState, 100, true);
}

/*
 * Main loop groove:
 * 1. Kick the schedulers in priority order so time-critical MIDI work happens first.
 *    - schedulerHigh → 1 ms tick: MIDI parsing, internal clock, arpeggiator.
 *    - schedulerMid  → 5–10 ms chores: serial command parsing and envelope tracking.
 *    - schedulerLow  → 50–100 ms eye candy: LEDs, filter tweaks, and display drawing.
 * 2. After the schedulers run, poll buttons and pots every spin for instant UI feel.
 * 3. Finish by checking system load so we know if we're pushing the MCU too hard.
 * Tasks never preempt each other; everyone plays nice and yields fast for the next riff.
 */
void loop() {
    Utility::schedulerHigh.update();
    Utility::schedulerMid.update();
    Utility::schedulerLow.update();
    buttonManager.processButtons(buttonContext);
    potentiometerManager.processPots(ledManager, envelopeFollowers);
    if (g_profileChangeRequested) {
        // Apply the active profile payload once the UI requests a change.
        ProfileData profile{};
        if (configManager.loadProfileSettings(g_activeProfile, profile)) {
            applyProfileSnapshot(profile, true);
        }
        g_profileChangeRequested = false;
    }
    if (g_profileSaveRequested) {
        // Snapshot current runtime state into the active profile slot.
        ProfileData profile = captureProfileSnapshot();
        configManager.saveProfileSettings(g_activeProfile, profile);
        g_profileSaveRequested = false;
    }
    monitorSystemLoad();
}
