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
  uint8_t lastNoteOn = 0;
  uint8_t lastNoteOnVelocity = 0;
  uint8_t lastNoteOnChannel = 0;
  uint8_t lastNoteOff = 0;
  uint8_t lastNoteOffVelocity = 0;
  uint8_t lastNoteOffChannel = 0;

  struct CCEvent { uint8_t control; uint8_t value; uint8_t channel; };
  CCEvent ccLog[8];
  uint8_t ccCount = 0;

  uint8_t lastSysEx[32];
  uint16_t lastSysExLength = 0;

  void begin(...) {}
  void sendControlChange(uint8_t control, uint8_t value, uint8_t channel) {
    if (ccCount < 8) ccLog[ccCount++] = {control, value, channel};
  }
  void sendNoteOn(uint8_t note, uint8_t velocity, uint8_t channel) {
    lastNoteOn = note;
    lastNoteOnVelocity = velocity;
    lastNoteOnChannel = channel;
  }
  void sendNoteOff(uint8_t note, uint8_t velocity, uint8_t channel) {
    lastNoteOff = note;
    lastNoteOffVelocity = velocity;
    lastNoteOffChannel = channel;
  }
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
  void sendSysEx(uint16_t length, const uint8_t* data, bool) {
    lastSysExLength = length > 32 ? 32 : length;
    for (uint16_t i = 0; i < lastSysExLength; ++i) lastSysEx[i] = data[i];
  }
  bool read() { return false; }
  midi::MidiType getType() { return midi::NoteOff; }
  uint8_t getChannel() { return 0; }
  uint8_t getData1() { return 0; }
  uint8_t getData2() { return 0; }
  const uint8_t* getSysExArray() { return lastSysEx; }
  uint16_t getSysExArrayLength() { return lastSysExLength; }
};

extern MidiInterfaceStub MIDI;
extern MidiInterfaceStub usbMIDI;

struct HardwareSerial {};
extern HardwareSerial Serial1;

#define MIDI_CREATE_INSTANCE(type, serial, name)
