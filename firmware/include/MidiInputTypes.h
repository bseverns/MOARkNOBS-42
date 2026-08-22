#ifndef MIDI_INPUT_TYPES_H
#define MIDI_INPUT_TYPES_H

#include <cstdint>

inline constexpr uint8_t MIDI_INPUT_MAX_BINDINGS = 16;

enum class MidiInputPort : uint8_t { Any = 0, Din = 1, Usb = 2 };
enum class MidiInputMode : uint8_t { Absolute = 0, Momentary = 1, Toggle = 2 };

enum class MachineParameterTarget : uint8_t {
    SlotValue = 0,
    ArpSwing,
    VelocityShift,
    NoteChance,
    ArpGate,
    JitterDepth,
    JitterSmoothness
};

inline constexpr uint8_t MIDI_INPUT_FLAG_SOFT_TAKEOVER = 0x01;

// Compact profile-owned route. Values and ranges use a normalized MIDI 0..127
// domain; MachineParameterService owns conversion to each destination's units.
struct __attribute__((packed)) MidiInputBinding {
    uint8_t port = static_cast<uint8_t>(MidiInputPort::Any);
    uint8_t channel = 1;
    uint8_t controller = 0;
    uint8_t target = static_cast<uint8_t>(MachineParameterTarget::SlotValue);
    uint8_t targetIndex = 0;
    uint8_t mode = static_cast<uint8_t>(MidiInputMode::Absolute);
    uint8_t minValue = 0;
    uint8_t maxValue = 127;
    uint8_t flags = MIDI_INPUT_FLAG_SOFT_TAKEOVER;
};

static_assert(sizeof(MidiInputBinding) == 9, "MIDI input binding storage drifted");

const char *midiInputPortName(MidiInputPort port);
const char *midiInputModeName(MidiInputMode mode);
const char *machineParameterTargetName(MachineParameterTarget target);
bool parseMidiInputPort(const char *name, MidiInputPort &port);
bool parseMidiInputMode(const char *name, MidiInputMode &mode);
bool parseMachineParameterTarget(const char *name, MachineParameterTarget &target,
                                 uint8_t &targetIndex);

#endif // MIDI_INPUT_TYPES_H
