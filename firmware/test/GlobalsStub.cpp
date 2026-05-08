#include "Globals.h"
#include "ConfigManager.h"
#include "PotentiometerManager.h"
#include "EnvelopeFollower.h"
#include "interop/SeedBoxLink.h"
#include "PerlinNoise.h"
#include "SysExTemplate.h"
#include "Utility.h"

#include <ArduinoJson.h>
#include <array>
#include <cctype>
#include <cmath>
#include <vector>

// Hardware snapshot so the tests can lean on mux pin definitions without
// hauling in the full Globals.cpp (and its SD/Serial baggage).
HardwareConfig hwConfig = {0,  0,  0, 16, 6, 8, NUM_BUTTONS, 1, 1, 1, 1, {2, 3, 4, 5}, {6, 7, 8, 9},
                           A0, A1, A2};

void loadHardwareConfig() {}

// Globals that the firmware normally defines in Globals.cpp.
uint32_t g_resetCause = 0;
uint16_t g_brownoutCount = 0;
float g_tappedBPM = 120.0f;
bool g_clockOutEnabled = false;
bool g_followExternalClock = true;
bool g_usbMidiOutEnabled = false;
unsigned long lastClockTime = 0;
uint8_t midiBeatPosition = 0;
char serialBuffer[SERIAL_BUFFER_SIZE] = {0};
uint8_t serialBufferIndex = 0;
float g_lfoEfGainTrim = 0.0f;
float g_lfoArpSwing = 0.0f;
float g_lfoVelocityShift = 0.0f;
float g_lfoNoteChance = 0.0f;
float g_lfoArpGate = 0.0f;
float g_lfoJitterDepth = 0.0f;
float g_lfoJitterSmoothness = 0.0f;
float g_lfoLedBrightness = 0.0f;
std::array<float, 2> g_lfoValues = {0.0f, 0.0f};
float g_vref = 3.3f;
EnvelopeConfig envelopeConfig = {};
SystemDiagnostics g_systemDiagnostics;
int8_t velocityShift = 0;
uint8_t changeProbability = 100;
JitterSettings g_jitterSettings = {1.0f, 0.5f};
bool g_jitterTuningActive = false;
bool g_arpEditActive = false; // Test shim for arp edit toggle
uint8_t g_activeProfile = 0;
bool g_profileChangeRequested = false;
bool g_profileSaveRequested = false;
uint8_t g_efIdleFloor = EF_IDLE_FLOOR_DEFAULT;
bool webSerialStreaming = false;

const std::array<int, NUM_ENVELOPES> ENVELOPE_ANALOG_PINS = {A0, A1, A2, A3, A6, A7};
namespace {
constexpr std::array<std::pair<uint8_t, uint8_t>, ARG_PAIR_COUNT> buildStubArgPairs() {
    std::array<std::pair<uint8_t, uint8_t>, ARG_PAIR_COUNT> pairs{};
    size_t idx = 0;
    for (uint8_t a = 0; a < ENVELOPE_ANALOG_PINS.size(); ++a) {
        for (uint8_t b = a + 1; b < ENVELOPE_ANALOG_PINS.size(); ++b) {
            pairs[idx++] = {a, b};
        }
    }
    return pairs;
}
} // namespace

const std::array<std::pair<uint8_t, uint8_t>, ARG_PAIR_COUNT> ARG_PAIRS = buildStubArgPairs();
extern const size_t ARG_PAIRS_LEN = ARG_PAIRS.size();

int envelopeAnalogPin(uint8_t index) {
    if (index < ENVELOPE_ANALOG_PINS.size())
        return ENVELOPE_ANALOG_PINS[index];
    return -1;
}

extern int envelopeIndexFromAnalogPin(int analogPin) {
    for (uint8_t idx = 0; idx < ENVELOPE_ANALOG_PINS.size(); ++idx) {
        if (ENVELOPE_ANALOG_PINS[idx] == analogPin)
            return idx;
    }
    return -1;
}

// The arpeggiator leans on Perlin noise for its RANDOM mode. The full noise
// implementation is overkill for these unit tests, so collapse it to a simple
// deterministic ramp that still exercises the call path.
float perlinNoise1D(float x) {
    // Keep the output in [-1, 1] while providing repeatable variation.
    float phase = std::fmod(x, 8.0f) / 8.0f;
    return 2.0f * phase - 1.0f;
}

#if !defined(UNIT_TEST_INTEROP_IMPL)
namespace seedbox {
namespace interop {
namespace mn42 {

SeedBoxLink &SeedBoxLink::instance() {
    static SeedBoxLink link;
    return link;
}

void SeedBoxLink::begin(::MIDIHandler *handler) {
    _midi = handler;
    _hasAck = false;
}

void SeedBoxLink::update() {}

bool SeedBoxLink::handleControlChange(uint8_t, uint8_t, uint8_t) { return false; }

void SeedBoxLink::handleSysEx(const uint8_t *, uint16_t) {}

bool SeedBoxLink::peerAlive() const { return false; }

void SeedBoxLink::sendHello() {}
void SeedBoxLink::sendAck() {}
void SeedBoxLink::sendKeepAlive() {}
void SeedBoxLink::sendIdentityPing() {}
void SeedBoxLink::markPeerPulse() {}

} // namespace mn42
} // namespace interop
} // namespace seedbox
#endif

#if !defined(UNIT_TEST_PROTOCOL_IMPL)
namespace {

bool equalsIgnoreCase(const char *lhs, const char *rhs) {
    if (!lhs || !rhs)
        return false;
    while (*lhs && *rhs) {
        if (std::tolower(static_cast<unsigned char>(*lhs)) !=
            std::tolower(static_cast<unsigned char>(*rhs))) {
            return false;
        }
        ++lhs;
        ++rhs;
    }
    return *lhs == '\0' && *rhs == '\0';
}

bool parseMIDITypeLabel(const char *label, MIDIMessageType &type) {
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

bool assignFromIntegral(long candidate, MIDIMessageType &type) {
    if (candidate < static_cast<long>(MIDIMessageType::OFF) ||
        candidate > static_cast<long>(MIDIMessageType::SysEx)) {
        return false;
    }
    type = static_cast<MIDIMessageType>(candidate);
    return true;
}

void clearSysExTemplate(MIDISlot &slot) {
    slot.sysexLength = 0;
    slot.sysexTemplate.fill(0);
}

uint8_t renderFallbackSysEx(const MIDISlot &slot, uint16_t rawValue, uint8_t *dest,
                            size_t capacity) {
    if (capacity < 4) {
        return 0;
    }
    dest[0] = 0xF0;
    dest[1] = slot.data1;
    dest[2] = Utility::mapToMidiValue(static_cast<int>(rawValue));
    dest[3] = 0xF7;
    return 4;
}

} // namespace

bool testOnly_parseSlotType(JsonVariantConst typeField, JsonVariantConst typeNameField,
                            MIDIMessageType &type) {
    if (!typeField.isNull()) {
        if (typeField.is<const char *>()) {
            if (parseMIDITypeLabel(typeField.as<const char *>(), type)) {
                return true;
            }
        } else if (typeField.is<int>() || typeField.is<long>() || typeField.is<short>() ||
                   typeField.is<signed char>()) {
            if (assignFromIntegral(typeField.as<long>(), type)) {
                return true;
            }
        } else if (typeField.is<unsigned char>() || typeField.is<unsigned short>() ||
                   typeField.is<unsigned int>() || typeField.is<unsigned long>()) {
            unsigned long raw = typeField.as<unsigned long>();
            if (raw <= static_cast<unsigned long>(static_cast<long>(MIDIMessageType::SysEx)) &&
                assignFromIntegral(static_cast<long>(raw), type)) {
                return true;
            }
        } else if (typeField.is<float>() || typeField.is<double>()) {
            double raw = typeField.as<double>();
            if (std::isfinite(raw)) {
                long candidate = static_cast<long>(raw);
                if (static_cast<double>(candidate) == raw && assignFromIntegral(candidate, type)) {
                    return true;
                }
            }
        }
    }

    if (!typeNameField.isNull() && typeNameField.is<const char *>()) {
        return parseMIDITypeLabel(typeNameField.as<const char *>(), type);
    }
    return false;
}

bool testOnly_parseSysExTemplateField(JsonVariantConst value, MIDISlot &slot, String &error) {
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

uint8_t testOnly_buildSysExPayload(const MIDISlot &slot, uint16_t rawValue, uint8_t *dest,
                                   size_t capacity) {
    const uint8_t midiValue = Utility::mapToMidiValue(static_cast<int>(rawValue));
    const uint16_t value14 = Utility::mapTo14Bit(static_cast<int>(rawValue));
    if (slot.sysexLength >= 2) {
        uint8_t rendered = SysExTemplate::render(slot.sysexTemplate, slot.sysexLength, midiValue,
                                                 value14, dest, capacity);
        if (rendered > 0) {
            return rendered;
        }
    }
    return renderFallbackSysEx(slot, rawValue, dest, capacity);
}
#endif
