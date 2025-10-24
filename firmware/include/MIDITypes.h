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

/**
 * Slot-scoped arithmetic routing grid (ARG) settings.
 *
 * Each slot can optionally splice two envelope followers together using
 * a method lifted straight from the legacy global ARG engine.  We keep
 * the payload tiny so it still fits neatly inside the EEPROM footprint
 * without blowing up the slot struct.
 */
enum class ARGMethod : uint8_t {
    PLUS = 0,
    MIN,
    PECK,
    SHAV,
    SQAR,
    BABS,
    TABS,
    MULT,
    DIVI,
    AVG,
    XABS,
    MAXX,
    MINN,
    XORR,
};

struct SlotARGConfig {
    uint8_t enabled = 0;          //!< Non-zero enables ARG blending for the slot
    ARGMethod method = ARGMethod::PLUS; //!< Math trick to apply when enabled
    uint8_t sourceA = 0;          //!< Envelope follower index feeding input A
    uint8_t sourceB = 1;          //!< Envelope follower index feeding input B
};

/** Configuration for a single pot slot. */
struct MIDISlot {
    MIDIMessageType type = MIDIMessageType::OFF;
    uint8_t midiChannel = 1;
    uint8_t data1 = 0;
    uint8_t efIndex = 0;
    bool active = false;
    uint8_t arpNote = 0; //!< Base note for arpeggiator
    uint8_t sysexLength = 0;
    std::array<uint8_t, SysExTemplate::kMaxLength> sysexTemplate{};
    SlotARGConfig arg{}; //!< Slot-local ARG mixer settings
};

constexpr uint8_t NUM_SLOTS = 42;

static_assert(sizeof(MIDISlot) <= 32, "MIDISlot exploded past the expected 32 bytes");

#endif // MIDI_TYPES_H
