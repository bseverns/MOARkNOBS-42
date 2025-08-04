#pragma once
#include <cstdint>

namespace midi {
enum MidiType : uint8_t {
  NoteOff,
  NoteOn,
  AfterTouchPoly,
  ControlChange,
  ProgramChange,
  AfterTouchChannel,
  PitchBend,
  SystemExclusive,
  Clock
};
}

struct MidiInterfaceStub {
  uint8_t lastProgram = 0;
  uint8_t lastProgramChannel = 0;
  uint8_t lastAftertouch = 0;
  uint8_t lastAftertouchChannel = 0;
  int16_t lastPitchBend = 0;
  uint8_t lastPitchBendChannel = 0;

  void begin(...) {}
  void sendControlChange(uint8_t, uint8_t, uint8_t) {}
  void sendNoteOn(uint8_t, uint8_t, uint8_t) {}
  void sendNoteOff(uint8_t, uint8_t, uint8_t) {}
  void sendProgramChange(uint8_t program, uint8_t channel) {
    lastProgram = program; lastProgramChannel = channel;
  }
  void sendAfterTouch(uint8_t pressure, uint8_t channel) {
    lastAftertouch = pressure; lastAftertouchChannel = channel;
  }
  void sendPitchBend(int bend, uint8_t channel) {
    lastPitchBend = bend; lastPitchBendChannel = channel;
  }
  void sendClock() {}
  void sendSysEx(uint16_t, const uint8_t*, bool) {}
  bool read() { return false; }
  midi::MidiType getType() { return midi::NoteOff; }
  uint8_t getChannel() { return 0; }
  uint8_t getData1() { return 0; }
  uint8_t getData2() { return 0; }
  const uint8_t* getSysExArray() { return nullptr; }
  uint16_t getSysExArrayLength() { return 0; }
};

extern MidiInterfaceStub MIDI;
extern MidiInterfaceStub usbMIDI;

struct HardwareSerial {};
extern HardwareSerial Serial1;

#define MIDI_CREATE_INSTANCE(type, serial, name)
