#pragma once

// Preempt the Teensy core's usb_midi.h so our stub keeps center stage.
// The core has shipped with a grab bag of include guards over the years, so
// we spray them all here to slam the door on any version that wanders in.
#ifndef USB_MIDI_H
#define USB_MIDI_H
#endif
#ifndef USB_MIDI_H_
#define USB_MIDI_H_
#endif
#ifndef usb_midi_h
#define usb_midi_h
#endif
#ifndef usb_midi_h_
#define usb_midi_h_
#endif
#ifndef usb_midi_h__
#define usb_midi_h__
#endif
#ifndef __USB_MIDI_H__
#define __USB_MIDI_H__
#endif
#ifndef __usb_midi_h__
#define __usb_midi_h__
#endif

// Spin up these lightweight MIDI stubs only when the unit-test rig asks for
// them *and* the USB_MIDI_STUB flag is set. The real Teensy core drags in its
// own usbMIDI guts, so we bail out unless both flags are waving.
#if defined(UNIT_TEST) && defined(USB_MIDI_STUB)
#include <cstdint>
#include <Arduino.h>

#ifdef usb_midi_class
#undef usb_midi_class
#endif
#ifdef usbMIDI
#undef usbMIDI
#endif

static constexpr uint16_t kSysExCapacity = 128;

namespace midi {
// Mirror the real library's event codes so tests match reality.
enum MidiType : uint8_t {
    InvalidType = 0x00,
    NoteOff = 0x80,
    NoteOn = 0x90,
    AfterTouchPoly = 0xA0,
    ControlChange = 0xB0,
    ProgramChange = 0xC0,
    AfterTouchChannel = 0xD0,
    PitchBend = 0xE0,
    SystemExclusive = 0xF0,
    TimeCodeQuarterFrame = 0xF1,
    SongPosition = 0xF2,
    SongSelect = 0xF3,
    Undefined_F4 = 0xF4,
    Undefined_F5 = 0xF5,
    TuneRequest = 0xF6,
    EndOfExclusive = 0xF7,
    Clock = 0xF8,
    Tick = Clock, // real-time clock tick
    Undefined_F9 = 0xF9,
    Start = 0xFA,
    Continue = 0xFB,
    Stop = 0xFC,
    Undefined_FD = 0xFD,
    ActiveSensing = 0xFE,
    SystemReset = 0xFF
};
} // namespace midi

struct MidiInterfaceStub {
    static constexpr size_t kCCLogCapacity = 128;
    static constexpr size_t kSysExCapacity = 256;
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

    struct CCEvent {
        uint8_t control;
        uint8_t value;
        uint8_t channel;
    };
    CCEvent ccLog[kCCLogCapacity];
    uint8_t ccCount = 0;
    uint32_t ccTotal = 0;
    bool ccOverflow = false;

    uint8_t lastSysEx[kSysExCapacity];
    uint16_t lastSysExLength = 0;
    uint16_t sysExTotal = 0;
    bool sysExOverflow = false;

    void begin(...) {}
    void sendControlChange(uint8_t control, uint8_t value, uint8_t channel);
    void sendNoteOn(uint8_t note, uint8_t velocity, uint8_t channel);
    void sendNoteOff(uint8_t note, uint8_t velocity, uint8_t channel);
    void sendProgramChange(uint8_t program, uint8_t channel);
    void sendAfterTouch(uint8_t pressure, uint8_t channel);
    void sendPitchBend(int bend, uint8_t channel);
    void sendClock();
    void sendSysEx(uint16_t length, const uint8_t *data, bool);
    bool read() { return false; }
    midi::MidiType getType() { return midi::InvalidType; }
    uint8_t getChannel() { return 0; }
    uint8_t getData1() { return 0; }
    uint8_t getData2() { return 0; }
    const uint8_t *getSysExArray() { return lastSysEx; }
    uint16_t getSysExArrayLength() { return lastSysExLength; }
};

// Mirror the real Teensy type name so downstream code doesn't know the
// difference.  Keep the class lean; we just log the bits we care about.
struct usb_midi_class : MidiInterfaceStub {
    bool nextRead = false;
    midi::MidiType nextType = midi::InvalidType;
    bool read() {
        bool r = nextRead;
        nextRead = false;
        return r;
    }
    midi::MidiType getType() { return nextType; }
};

extern MidiInterfaceStub MIDI;
extern usb_midi_class usbMIDI;

#ifndef MIDI_CREATE_INSTANCE
#define MIDI_CREATE_INSTANCE(type, serial, name)
#endif
#else
#include_next "usb_midi.h"
#endif // defined(UNIT_TEST) && defined(USB_MIDI_STUB)

#if defined(UNIT_TEST) && defined(USB_MIDI_STUB)
inline void MidiInterfaceStub::sendControlChange(uint8_t control, uint8_t value, uint8_t channel) {
    ++ccTotal;
    if (ccCount < 8) {
        ccLog[ccCount++] = {control, value, channel};
    } else {
        ccOverflow = true;
    }
    if (this != reinterpret_cast<MidiInterfaceStub *>(&usbMIDI)) {
        usbMIDI.sendControlChange(control, value, channel);
    }
}

inline void MidiInterfaceStub::sendNoteOn(uint8_t note, uint8_t velocity, uint8_t channel) {
    lastNoteOn = note;
    lastNoteOnVelocity = velocity;
    lastNoteOnChannel = channel;
    if (this != reinterpret_cast<MidiInterfaceStub *>(&usbMIDI)) {
        usbMIDI.sendNoteOn(note, velocity, channel);
    }
}

inline void MidiInterfaceStub::sendNoteOff(uint8_t note, uint8_t velocity, uint8_t channel) {
    lastNoteOff = note;
    lastNoteOffVelocity = velocity;
    lastNoteOffChannel = channel;
    if (this != reinterpret_cast<MidiInterfaceStub *>(&usbMIDI)) {
        usbMIDI.sendNoteOff(note, velocity, channel);
    }
}

inline void MidiInterfaceStub::sendProgramChange(uint8_t program, uint8_t channel) {
    lastProgram = program;
    lastProgramChannel = channel;
    if (this != reinterpret_cast<MidiInterfaceStub *>(&usbMIDI)) {
        usbMIDI.sendProgramChange(program, channel);
    }
}

inline void MidiInterfaceStub::sendAfterTouch(uint8_t pressure, uint8_t channel) {
    lastAftertouch = pressure;
    lastAftertouchChannel = channel;
    if (this != reinterpret_cast<MidiInterfaceStub *>(&usbMIDI)) {
        usbMIDI.sendAfterTouch(pressure, channel);
    }
}

inline void MidiInterfaceStub::sendPitchBend(int bend, uint8_t channel) {
    lastPitchBend = bend;
    lastPitchBendChannel = channel;
    if (this != reinterpret_cast<MidiInterfaceStub *>(&usbMIDI)) {
        usbMIDI.sendPitchBend(bend, channel);
    }
}

inline void MidiInterfaceStub::sendClock() {
    if (this != reinterpret_cast<MidiInterfaceStub *>(&usbMIDI)) {
        usbMIDI.sendClock();
    }
}

inline void MidiInterfaceStub::sendSysEx(uint16_t length, const uint8_t *data, bool) {
    sysExTotal = length;
    lastSysExLength = length > kSysExCapacity ? kSysExCapacity : length;
    sysExOverflow = length > kSysExCapacity;
    for (uint16_t i = 0; i < lastSysExLength; ++i) {
        lastSysEx[i] = data[i];
    }
    if (this != reinterpret_cast<MidiInterfaceStub *>(&usbMIDI)) {
        usbMIDI.sendSysEx(length, data, true);
    }
}
#endif
