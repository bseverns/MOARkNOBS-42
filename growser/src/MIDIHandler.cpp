#include "MIDIHandler.h"

MIDI_CREATE_INSTANCE(HardwareSerial, Serial1, MIDI);

MIDIHandler::MIDIHandler() {}

void MIDIHandler::begin() {
    MIDI.begin(MIDI_CHANNEL_OMNI);
}

void MIDIHandler::sendControlChange(uint8_t control, uint8_t value, uint8_t channel) {
    MIDI.sendControlChange(control, value, channel);
}

//note on/off for future, sequencer of 60+ notes?
void MIDIHandler::sendNoteOn(uint8_t note, uint8_t velocity, uint8_t channel) {
    MIDI.sendNoteOn(note, velocity, channel);
}

void MIDIHandler::sendNoteOff(uint8_t note, uint8_t velocity, uint8_t channel) {
    MIDI.sendNoteOff(note, velocity, channel);
}

void MIDIHandler::processIncomingMIDI() {
    if (MIDI.read()) {
        switch (MIDI.getType()) {
        case midi::ControlChange:
            handleControlChange(MIDI.getChannel(), MIDI.getData1(), MIDI.getData2());
            break;
        case midi::NoteOn:
            handleNoteOn(MIDI.getChannel(), MIDI.getData1(), MIDI.getData2());
            break;
        case midi::NoteOff:
            handleNoteOff(MIDI.getChannel(), MIDI.getData1(), MIDI.getData2());
            break;
        default:
            break;
        }
    }
}

void MIDIHandler::handleControlChange(uint8_t channel, uint8_t control, uint8_t value) {
    // Custom logic for handling incoming CC messages
}

void MIDIHandler::handleNoteOn(uint8_t channel, uint8_t note, uint8_t velocity) {
    // Custom logic for handling incoming Note On messages
}

void MIDIHandler::handleNoteOff(uint8_t channel, uint8_t note, uint8_t velocity) {
    // Custom logic for handling incoming Note Off messages
}

bool MIDIHandler::isClockTick() {
    return MIDI.getType() == midi::Clock;
}