// include/MidiTypes.h
#ifndef MIDI_TYPES_H
#define MIDI_TYPES_H

#include <cstdint>

/** Supported MIDI message types for a slot. */
enum class MIDIMessageType : uint8_t {
  OFF = 0, //!< Slot disabled
  CC,     //!< Control Change
  Note,   //!< Note on/off
  PitchBend,
  ProgramChange,
  Aftertouch,
  NRPN,   //!< Non-Registered Parameter Number send
  SysEx   //!< System Exclusive thrasher
};

/** Configuration for a single pot slot. */
struct MIDISlot {
  MIDIMessageType type;
  uint8_t        midiChannel;
  uint8_t        data1;
  uint8_t        efIndex;
  bool           active;
  uint8_t        arpNote;   //!< Base note for arpeggiator
};

constexpr uint8_t NUM_SLOTS = 42;

#endif // MIDI_TYPES_H
