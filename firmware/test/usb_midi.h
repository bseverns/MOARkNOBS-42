#pragma once

// Spin up these lightweight MIDI stubs only when the unit-test rig asks for
// them *and* the USB_MIDI_STUB flag is set. The real Teensy core drags in its
// own usbMIDI guts, so we bail out unless both flags are waving.
#if defined(UNIT_TEST) && defined(USB_MIDI_STUB)
// Hijack the core's usbMIDI symbol before any Arduino headers get a shot.
#define usbMIDI usbMIDI_real
#include <cstdint>

namespace midi {
// Mirror the real library's event codes so tests match reality.
enum MidiType : uint8_t {
  InvalidType           = 0x00,
  NoteOff               = 0x80,
  NoteOn                = 0x90,
  AfterTouchPoly        = 0xA0,
  ControlChange         = 0xB0,
  ProgramChange         = 0xC0,
  AfterTouchChannel     = 0xD0,
  PitchBend             = 0xE0,
  SystemExclusive       = 0xF0,
  TimeCodeQuarterFrame  = 0xF1,
  SongPosition          = 0xF2,
  SongSelect            = 0xF3,
  Undefined_F4          = 0xF4,
  Undefined_F5          = 0xF5,
  TuneRequest           = 0xF6,
  EndOfExclusive        = 0xF7,
  Clock                 = 0xF8,
  Tick                  = Clock, // real-time clock tick
  Undefined_F9          = 0xF9,
  Start                 = 0xFA,
  Continue              = 0xFB,
  Stop                  = 0xFC,
  Undefined_FD          = 0xFD,
  ActiveSensing         = 0xFE,
  SystemReset           = 0xFF
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
  midi::MidiType getType() { return midi::InvalidType; }
  uint8_t getChannel() { return 0; }
  uint8_t getData1() { return 0; }
  uint8_t getData2() { return 0; }
  const uint8_t* getSysExArray() { return lastSysEx; }
  uint16_t getSysExArrayLength() { return lastSysExLength; }
};

struct USBMidiStub : MidiInterfaceStub {
  bool nextRead = false;
  midi::MidiType nextType = midi::InvalidType;
  bool read() { bool r = nextRead; nextRead = false; return r; }
  midi::MidiType getType() { return nextType; }
};

extern MidiInterfaceStub MIDI;

#ifndef MIDI_CREATE_INSTANCE
#define MIDI_CREATE_INSTANCE(type, serial, name)
#endif
#else
#include_next "usb_midi.h"
#endif  // defined(UNIT_TEST)
