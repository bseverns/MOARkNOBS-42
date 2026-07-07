// include/MidiTypes.h
#ifndef MIDI_TYPES_H
#define MIDI_TYPES_H

#include <array>
#include <cstdint>

#include "SysExTemplate.h"

// Supported MIDI message types for a slot.
enum class MIDIMessageType : uint8_t {
    OFF = 0, // Slot disabled
    CC,      // Control Change
    Note,    // Note on/off
    PitchBend,
    ProgramChange,
    Aftertouch,
    ModWheel, // Modulation wheel (CC1)
    NRPN,     // Non-Registered Parameter Number send
    RPN,      // Registered Parameter Number send
    SysEx     // System Exclusive thrasher
};

// Classification for the most recent SysEx packet.
enum class SysExType : uint8_t {
    ManufacturerSpecific = 0, // Vendor IDs other than 7E/7F
    UniversalNonRealTime,     // 0x7E per the MIDI spec
    UniversalRealTime         // 0x7F per the MIDI spec
};

// Enumerates how the per-slot ARG mixer should blend its envelope pair.
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

// How a slot-local EF contribution is folded into the outgoing MIDI value.
enum class EfDestinationMode : uint8_t {
    AddClamp = 0, // Current default: base pot value + EF contribution, clamped 0..127
    Subtract,     // Base pot value - EF contribution, clamped 0..127
    Replace,      // EF contribution replaces the base pot value
    Scale,        // EF contribution scales the base pot value
    Centered,     // EF contribution is treated as a centered bipolar offset
};

// Envelope follower payload persisted alongside the slot definition.
struct SlotEnvelopePayload {
    uint8_t filterType = 0; // EnvelopeFollower::FilterType value
    float frequency = 0.0f; // Stored cutoff/shape frequency (Hz or scalar)
    float q = 0.0f;         // Stored resonance/Q for biquad-driven modes
};

// Slot-local configuration for the ARG combiner.
struct SlotARGConfig {
    uint8_t enabled = 0;                // Non-zero enables ARG processing for this slot
    ARGMethod method = ARGMethod::PLUS; // Mixer math to apply when ARG is on
    uint8_t sourceA = 0;                // Primary envelope index
    uint8_t sourceB = 1;                // Secondary envelope index
};

struct MIDISlot {
    // Envelope follower configuration scoped to this slot.
    struct EfSettings {
        // Filter shapes mirrored from EnvelopeFollower::FilterType.
        enum class FilterType : uint8_t {
            Linear = 0,
            OppositeLinear,
            Exponential,
            Random,
            Lowpass,
            Highpass,
            Bandpass
        };

        int8_t followerIndex = -1; // Assigned hardware follower (-1 when unbound)
        uint8_t oversample = 4;    // ADC oversample count (1 == disabled)
        FilterType filterType = FilterType::Linear;
        uint8_t efMode = 0;            // EnvelopeFollower::EFMode value
        uint8_t autoBaseline = 1;      // Enable auto-baseline tracking
        uint8_t autoGain = 1;          // Enable auto-gain tracking
        uint8_t gateThreshold = 16;    // Gate threshold (0-127)
        uint8_t gateHysteresis = 4;    // Gate hysteresis (0-127)
        uint8_t activityThreshold = 4; // Activity threshold (0-127)
        uint8_t gainTarget = 102;      // Auto-gain target (0-127)
        uint8_t destinationMode =
            static_cast<uint8_t>(EfDestinationMode::AddClamp); // EfDestinationMode value
        uint16_t attackMs = 5;         // Attack time for Peak/Follower modes
        uint16_t releaseMs = 20;       // Release time for Peak/Follower modes
        uint16_t rmsWindowMs = 50;     // RMS integration window
        uint16_t baselineTauMs = 2000; // Auto-baseline time constant
        uint16_t gainTauMs = 3000;     // Auto-gain time constant
        float frequency = 1000.0f;     // Cutoff/shape frequency in Hz
        float q = 0.707f;              // Resonance / secondary filter parameter
        float smoothing = 0.2f;        // EWMA smoothing factor (0..1)
        float baseline = 0.0f;         // Noise floor offset after calibration
        float gain = 1.0f;             // Output gain applied post-baseline
    };

    MIDIMessageType type = MIDIMessageType::OFF;
    uint8_t midiChannel = 1;
    uint8_t data1 = 0;
    bool active = false;
    uint8_t arpNote = 0; // Base note for arpeggiator
    uint8_t sysexLength = 0;
    EfSettings efSettings{}; // Slot-specific envelope follower settings
    std::array<uint8_t, SysExTemplate::kMaxLength> sysexTemplate{};
    struct EfRuntime {
        int8_t followerIndex = -1; // Currently assigned hardware follower (-1 when unbound)
    };
    EfRuntime ef{};      // Live envelope follower assignment metadata
    SlotARGConfig arg{}; // Slot-local ARG mixer settings

    // Update both persistent and runtime follower assignments in lockstep.
    void setEnvelopeFollowerIndex(int8_t index) {
        ef.followerIndex = index;
        efSettings.followerIndex = index;
    }

    // Convenience accessor mirroring the currently assigned follower index.
    int8_t getEnvelopeFollowerIndex() const { return ef.followerIndex; }
};

constexpr uint8_t NUM_SLOTS = 42;
inline constexpr uint8_t EF_OVERSAMPLE_MIN = 1;
inline constexpr uint8_t EF_OVERSAMPLE_MAX = 32;
inline constexpr uint16_t EF_TIME_MIN_MS = 1;
inline constexpr uint16_t EF_TIME_MAX_MS = 60000;
inline constexpr float EF_FILTER_FREQ_MIN_HZ = 20.0f;
inline constexpr float EF_FILTER_FREQ_MAX_HZ = 5000.0f;
inline constexpr float EF_FILTER_Q_MIN = 0.5f;
inline constexpr float EF_FILTER_Q_MAX = 4.0f;

// The slot struct picked up a richer envelope follower payload which nudged it
// past the original 64-byte budget. We still guard the footprint, but give it a
// little more breathing room so the build doesn't implode every time we add a
// tuning parameter.
static_assert(sizeof(MIDISlot) <= 80, "MIDISlot exploded past the expected 80 bytes");

#endif // MIDI_TYPES_H
