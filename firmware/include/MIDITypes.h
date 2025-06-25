// include/MidiTypes.h
#ifndef MIDI_TYPES_H
#define MIDI_TYPES_H

#include <cstdint>

enum class MIDIMessageType : uint8_t {
  OFF = 0, CC, Note, PitchBend, ProgramChange, Aftertouch
};

struct MIDISlot {
  MIDIMessageType type;
  uint8_t        midiChannel;
  uint8_t        data1;
  uint8_t        efIndex;
  bool           active;
};

constexpr uint8_t NUM_SLOTS = 42;

#endif // MIDI_TYPES_H