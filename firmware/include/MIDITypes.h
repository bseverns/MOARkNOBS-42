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
    uint8_t enabled = 0;                //!< Non-zero enables ARG blending for the slot
    ARGMethod method = ARGMethod::PLUS; //!< Math trick to apply when enabled
    uint8_t sourceA = 0;                //!< Envelope follower index feeding input A
    uint8_t sourceB = 1;                //!< Envelope follower index feeding input B
};

/**
 * Envelope follower settings persisted with each slot.
 *
 * The struct lives at global scope so helper functions can talk about a slot's
 * configuration without dragging MIDISlot itself into the conversation.  The
 * slot struct aliases this record so existing code using MIDISlot::EfSettings
 * keeps working.
 */
struct EfSettings {
    /** Filter shapes mirrored from EnvelopeFollower::FilterType. */
    enum class FilterType : uint8_t {
        Linear = 0,
        OppositeLinear,
        Exponential,
        Random,
        Lowpass,
        Highpass,
        Bandpass
    };

    FilterType filterType = FilterType::Linear; //!< Desired filter topology
    int8_t followerIndex = -1;                  //!< Assigned hardware follower (-1 when unbound)
    uint8_t oversample = 4;                     //!< ADC oversample count (1 == disabled)
    float frequency = 1000.0f;                  //!< Cutoff/shape frequency in Hz
    float q = 0.707f;                           //!< Resonance / secondary filter parameter
    float smoothing = 0.2f;                     //!< EWMA smoothing factor (0..1)
    float baseline = 0.0f;                      //!< Noise floor offset after calibration
    float gain = 1.0f;                          //!< Output gain applied post-baseline
};

struct MIDISlot {
    using EfSettings = ::EfSettings;

    MIDIMessageType type = MIDIMessageType::OFF;
    uint8_t midiChannel = 1;
    uint8_t data1 = 0;
    bool active = false;
    uint8_t arpNote = 0; //!< Base note for arpeggiator
    uint8_t sysexLength = 0;
    EfSettings efSettings{}; //!< Slot-specific envelope follower settings
    std::array<uint8_t, SysExTemplate::kMaxLength> sysexTemplate{};
    EfSettings ef{};     //!< Local envelope follower configuration
    SlotARGConfig arg{}; //!< Slot-local ARG mixer settings
};

constexpr uint8_t NUM_SLOTS = 42;

static_assert(sizeof(MIDISlot) <= 96, "MIDISlot exploded past the expected 96 bytes");

#endif // MIDI_TYPES_H
