#pragma once

// Only compile these lightweight MIDI stubs when the Arduino core is
// missing or we're running under a test harness. Real hardware brings
// its own USB-MIDI definitions and we keep out of its way.
#if !defined(ARDUINO) || defined(UNIT_TEST)
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

  struct CCEvent { uint8_t control; uint8_t value; uint8_t channel; };
  CCEvent ccLog[8];
  uint8_t ccCount = 0;

  uint8_t lastSysEx[32];
  uint16_t lastSysExLength = 0;

  void begin(...) {}
  void sendControlChange(uint8_t control, uint8_t value, uint8_t channel) {
    if (ccCount < 8) ccLog[ccCount++] = {control, value, channel};
  }
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

struct USBMidiStub : MidiInterfaceStub {
  bool nextRead = false;
  midi::MidiType nextType = midi::NoteOff;
  bool read() { bool r = nextRead; nextRead = false; return r; }
  midi::MidiType getType() { return nextType; }
};

extern MidiInterfaceStub MIDI;
extern USBMidiStub usbMIDI;

struct HardwareSerial {};
extern HardwareSerial Serial1;

#define MIDI_CREATE_INSTANCE(type, serial, name)
#endif  // !defined(ARDUINO)
