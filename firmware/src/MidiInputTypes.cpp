#include "MidiInputTypes.h"

#include <cctype>
#include <cstdio>
#include <cstring>

namespace {
bool equalIgnoreCase(const char *lhs, const char *rhs) {
    if (!lhs || !rhs) return false;
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
} // namespace

const char *midiInputPortName(MidiInputPort port) {
    switch (port) {
    case MidiInputPort::Din: return "din";
    case MidiInputPort::Usb: return "usb";
    case MidiInputPort::Any: return "any";
    }
    return "any";
}

const char *midiInputModeName(MidiInputMode mode) {
    switch (mode) {
    case MidiInputMode::Momentary: return "momentary";
    case MidiInputMode::Toggle: return "toggle";
    case MidiInputMode::Absolute: return "absolute";
    }
    return "absolute";
}

const char *machineParameterTargetName(MachineParameterTarget target) {
    switch (target) {
    case MachineParameterTarget::SlotValue: return "slot.value";
    case MachineParameterTarget::ArpSwing: return "arp.swing";
    case MachineParameterTarget::VelocityShift: return "note.velocity_shift";
    case MachineParameterTarget::NoteChance: return "note.probability";
    case MachineParameterTarget::ArpGate: return "arp.gate";
    case MachineParameterTarget::JitterDepth: return "jitter.depth";
    case MachineParameterTarget::JitterSmoothness: return "jitter.smoothness";
    }
    return "slot.value";
}

bool parseMidiInputPort(const char *name, MidiInputPort &port) {
    if (equalIgnoreCase(name, "any")) port = MidiInputPort::Any;
    else if (equalIgnoreCase(name, "din")) port = MidiInputPort::Din;
    else if (equalIgnoreCase(name, "usb")) port = MidiInputPort::Usb;
    else return false;
    return true;
}

bool parseMidiInputMode(const char *name, MidiInputMode &mode) {
    if (equalIgnoreCase(name, "absolute")) mode = MidiInputMode::Absolute;
    else if (equalIgnoreCase(name, "momentary")) mode = MidiInputMode::Momentary;
    else if (equalIgnoreCase(name, "toggle")) mode = MidiInputMode::Toggle;
    else return false;
    return true;
}

bool parseMachineParameterTarget(const char *name, MachineParameterTarget &target,
                                 uint8_t &targetIndex) {
    if (!name) return false;
    unsigned index = 0;
    char trailing = '\0';
    if (std::sscanf(name, "slot.%u.value%c", &index, &trailing) == 1 && index < 42) {
        target = MachineParameterTarget::SlotValue;
        targetIndex = static_cast<uint8_t>(index);
        return true;
    }
    targetIndex = 0;
    if (equalIgnoreCase(name, "arp.swing")) target = MachineParameterTarget::ArpSwing;
    else if (equalIgnoreCase(name, "note.velocity_shift"))
        target = MachineParameterTarget::VelocityShift;
    else if (equalIgnoreCase(name, "note.probability")) target = MachineParameterTarget::NoteChance;
    else if (equalIgnoreCase(name, "arp.gate")) target = MachineParameterTarget::ArpGate;
    else if (equalIgnoreCase(name, "jitter.depth")) target = MachineParameterTarget::JitterDepth;
    else if (equalIgnoreCase(name, "jitter.smoothness"))
        target = MachineParameterTarget::JitterSmoothness;
    else return false;
    return true;
}
