#include "MIDIHandler.h"

MIDI_CREATE_INSTANCE(HardwareSerial, Serial1, MIDI);

MIDIHandler::MIDIHandler() {}

void MIDIHandler::begin() {
    MIDI.begin(MIDI_CHANNEL_OMNI);
    usbMIDI.begin();
}

void MIDIHandler::sendControlChange(uint8_t control, uint8_t value, uint8_t channel) {
    MIDI.sendControlChange(control, value, channel);
    usbMIDI.sendControlChange(control, value, channel);  // USB MIDI
    
}

//note on/off for future, sequencer of 60+ notes?
void MIDIHandler::sendNoteOn(uint8_t note, uint8_t velocity, uint8_t channel) {
    MIDI.sendNoteOn(note, velocity, channel);
    usbMIDI.sendNoteOn(note, velocity, channel);  // USB MIDI
    
}

void MIDIHandler::sendNoteOff(uint8_t note, uint8_t velocity, uint8_t channel) {
    MIDI.sendNoteOff(note, velocity, channel);
    usbMIDI.sendNoteOff(note, velocity, channel);  // USB MIDI
    
}

void MIDIHandler::processIncomingMIDI() {
    // Process Serial MIDI
    while (MIDI.read()) {
        Serial.println("Serial MIDI Message Received");
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

    // Process USB MIDI only if connected
    while (usbMIDI.read()) {
        Serial.println("USB MIDI Message Received");
        switch (usbMIDI.getType()) {
            case usbMIDI.ControlChange:
                handleControlChange(usbMIDI.getChannel(), usbMIDI.getData1(), usbMIDI.getData2());
                break;
            case usbMIDI.NoteOn:
                handleNoteOn(usbMIDI.getChannel(), usbMIDI.getData1(), usbMIDI.getData2());
                break;
            case usbMIDI.NoteOff:
                handleNoteOff(usbMIDI.getChannel(), usbMIDI.getData1(), usbMIDI.getData2());
                break;
            default:
                break;
        }
    }
}

void MIDIHandler::handleControlChange(uint8_t channel, uint8_t control, uint8_t value) {
    MIDI.sendControlChange(control, value, channel);
    usbMIDI.sendControlChange(control, value, channel); 
}

void MIDIHandler::handleNoteOn(uint8_t channel, uint8_t note, uint8_t velocity) {
    MIDI.sendNoteOn(note, velocity, channel);
    usbMIDI.sendNoteOn(note, velocity, channel);
}

void MIDIHandler::handleNoteOff(uint8_t channel, uint8_t note, uint8_t velocity) {
    MIDI.sendNoteOff(note, velocity, channel);
    usbMIDI.sendNoteOff(note, velocity, channel);
}

bool MIDIHandler::isClockTick() {
    static bool clockTick = false;
    if (MIDI.getType() == midi::Clock) {
        clockTick = true;
    }
    return clockTick;
}

void MIDIHandler::clearClockTick() {
    clockTick = false;
}