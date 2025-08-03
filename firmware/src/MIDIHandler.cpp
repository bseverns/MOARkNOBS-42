// Thin wrapper around the Teensy MIDI libraries.
// Sends and receives messages while updating DisplayManager.
// Instantiated and used throughout firmware_main.cpp.

#include "MIDIHandler.h"
#include <USB-MIDI.h>

// Provides a small abstraction over both Serial and USB MIDI transports. Other
// modules call these helpers to send messages, while incoming data is routed to
// callbacks that update display and arpeggiator state.

MIDI_CREATE_INSTANCE(HardwareSerial, Serial1, MIDI);

MIDIHandler::MIDIHandler() {}

void MIDIHandler::begin() {
    MIDI.begin(MIDI_CHANNEL_OMNI);
    usbMIDI.begin();
}

void MIDIHandler::sendControlChange(uint8_t control, uint8_t value, uint8_t channel) {
    // Validate before sending
    if (control > 127 || value > 127 || channel < 1 || channel > 16)
        return;
    MIDI.sendControlChange(control, value, channel);
    usbMIDI.sendControlChange(control, value, channel);  // USB MIDI
}

void MIDIHandler::sendNoteOn(uint8_t note, uint8_t velocity, uint8_t channel) {
    if (note > 127 || velocity > 127 || channel < 1 || channel > 16)
        return;
    MIDI.sendNoteOn(note, velocity, channel);
    usbMIDI.sendNoteOn(note, velocity, channel);
}

void MIDIHandler::sendNoteOff(uint8_t note, uint8_t velocity, uint8_t channel) {
    if (note > 127 || velocity > 127 || channel < 1 || channel > 16)
        return;
    MIDI.sendNoteOff(note, velocity, channel);
    usbMIDI.sendNoteOff(note, velocity, channel);
}

void MIDIHandler::processIncomingMIDI() {
    // Serial MIDI is the crusty hardware port. When it spits out a full
    // message, read() returns true and we hurl the parsed bytes at
    // handleMIDI so the rest of the rig can jam.
    if (MIDI.read()) {
        handleMIDI(MIDI.getType(), MIDI.getChannel(), MIDI.getData1(), MIDI.getData2());
    }

    // USB MIDI stockpiles packets in a buffer. Drain that queue in a loop
    // so nothing gets stale, feeding each packet through the same handler as
    // the old-school wire.
    while (usbMIDI.read()) {
        handleMIDI(usbMIDI.getType(), usbMIDI.getChannel(), usbMIDI.getData1(), usbMIDI.getData2());
    }
    if (_displayManager) {
        _displayManager->registerInteraction();
    }
}

void MIDIHandler::handleMIDI(uint8_t type, uint8_t channel, uint8_t data1, uint8_t data2) {
    switch (type) {
        case midi::ControlChange:
            Serial.printf("CC: %d, Value: %d, Channel: %d\n", data1, data2, channel);
            break;
        case midi::NoteOn:
            handleNoteOn(channel, data1, data2);
            break;
        case midi::NoteOff:
            handleNoteOff(channel, data1, data2);
            break;
        default:
            Serial.println("Unhandled MIDI message");
            break;
    }
}

void MIDIHandler::handleNoteOn(uint8_t channel, uint8_t note, uint8_t velocity) {
    Serial.printf("Note On: %d, Velocity: %d, Channel: %d\n", note, velocity, channel);
    MIDI.sendNoteOn(note, velocity, channel);
    usbMIDI.sendNoteOn(note, velocity, channel);
}

void MIDIHandler::handleNoteOff(uint8_t channel, uint8_t note, uint8_t velocity) {
    Serial.printf("Note Off: %d, Velocity: %d, Channel: %d\n", note, velocity, channel);
    MIDI.sendNoteOff(note, velocity, channel);
    usbMIDI.sendNoteOff(note, velocity, channel);
}

// Did we just hear a MIDI clock pulse? Tempo tracker taps this.
bool MIDIHandler::isClockTick() {
    return MIDI.getType() == midi::Clock;
}

// Wipe the clock pulse flag so the next beat counts.
void MIDIHandler::clearClockTick() {
    clockTick = false;
}

void MIDIHandler::sendProgramChange(uint8_t program, uint8_t channel) {
  if (program>127|| channel<1||channel>16) return;
  MIDI.sendProgramChange(program, channel);
  usbMIDI.sendProgramChange(program, channel);
}

void MIDIHandler::sendAftertouch(uint8_t pressure, uint8_t channel) {
  if (pressure>127|| channel<1||channel>16) return;
  MIDI.sendAfterTouch(pressure, channel);
  usbMIDI.sendAfterTouch(pressure, channel);
}

void MIDIHandler::sendPitchBend(int16_t bend, uint8_t channel) {
  // Validate channel and clamp bend value
  if (channel < 1 || channel > 16) return;
  if (bend < -8192) bend = -8192;
  else if (bend > 8191) bend = 8191;

  // Teensy and USB MIDI libraries accept the signed 14-bit value directly
  MIDI.sendPitchBend(bend, channel);
  usbMIDI.sendPitchBend(bend, channel);
}
