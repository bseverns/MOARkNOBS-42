// include/MidiTypes.h
#ifndef MIDI_TYPES_H
#define MIDI_TYPES_H

#include <array>
#include <cstdint>

#include "SysExTemplate.h"

/** Supported MIDI message types for a slot. */
enum class MIDIMessageType : uint8_t {
    OFF = 0, //!< Slot disabled
    CC,      //!< Control Change
    Note,    //!< Note on/off
    PitchBend,
    ProgramChange,
    Aftertouch,
    ModWheel, //!< Modulation wheel (CC1)
    NRPN,     //!< Non-Registered Parameter Number send
    RPN,      //!< Registered Parameter Number send
    SysEx     //!< System Exclusive thrasher
};

/** Classification for the most recent SysEx packet. */
enum class SysExType : uint8_t {
    ManufacturerSpecific = 0, //!< Vendor IDs other than 7E/7F
    UniversalNonRealTime,     //!< 0x7E per the MIDI spec
    UniversalRealTime         //!< 0x7F per the MIDI spec
};

/** Configuration for a single pot slot. */
struct EfSettings {
    uint8_t filterType = 0;   //!< Encoded EnvelopeFollower::FilterType
    float frequency = 1000.0f; //!< Stored cutoff / shaping frequency
    float q = 0.707f;          //!< Stored resonance / shaping curve parameter
};

struct MIDISlot {
    MIDIMessageType type = MIDIMessageType::OFF;
    uint8_t midiChannel = 1;
    uint8_t data1 = 0;
    uint8_t efIndex = 0;
    bool active = false;
    uint8_t arpNote = 0; //!< Base note for arpeggiator
    uint8_t sysexLength = 0;
    EfSettings efSettings{}; //!< Slot-specific envelope follower settings
    std::array<uint8_t, SysExTemplate::kMaxLength> sysexTemplate{};
};

constexpr uint8_t NUM_SLOTS = 42;

static_assert(sizeof(MIDISlot) == 32, "MIDISlot exploded past the expected 32 bytes");

#endif // MIDI_TYPES_H
