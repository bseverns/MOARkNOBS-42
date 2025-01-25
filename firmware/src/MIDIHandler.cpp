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
    if (control > 127 || value > 127 || channel < 1 || channel > 16) {
    return; // Ignore invalid message
    }
}

void MIDIHandler::sendNoteOn(uint8_t note, uint8_t velocity, uint8_t channel) {
    MIDI.sendNoteOn(note, velocity, channel);
    usbMIDI.sendNoteOn(note, velocity, channel);  // USB MIDI
    if (note > 127 || velocity > 127 || channel < 1 || channel > 16) {
    return; // Ignore invalid message
    }
}

void MIDIHandler::sendNoteOff(uint8_t note, uint8_t velocity, uint8_t channel) {
    MIDI.sendNoteOff(note, velocity, channel);
    usbMIDI.sendNoteOff(note, velocity, channel);  // USB MIDI
    if (note > 127 || velocity > 127 || channel < 1 || channel > 16) {
    return; // Ignore invalid message
    }
}

void MIDIHandler::processIncomingMIDI() {
    // Process Serial MIDI
    if (MIDI.read()) {
        handleMIDI(
            MIDI.getType(),      // MIDI message type
            MIDI.getChannel(),   // Channel number
            MIDI.getData1(),     // Data1 (e.g., note or control)
            MIDI.getData2()      // Data2 (e.g., velocity or value)
        );
    }

    // Process USB MIDI
    while (usbMIDI.read()) {
        handleMIDI(
            usbMIDI.getType(),     // USB MIDI message type
            usbMIDI.getChannel(),  // Channel number
            usbMIDI.getData1(),    // Data1 (e.g., note or control)
            usbMIDI.getData2()     // Data2 (e.g., velocity or value)
        );
    }
}

void MIDIHandler::handleMIDI(uint8_t type, uint8_t channel, uint8_t data1, uint8_t data2) {
    switch (type) {
        case midi::ControlChange:
            Serial.printf("CC: %d, Value: %d, Channel: %d\n", data1, data2, channel);
            break;

        case midi::NoteOn:
            handleNoteOn(channel, data1, data2); // Delegate to Note On handler
            break;

        case midi::NoteOff:
            handleNoteOff(channel, data1, data2); // Delegate to Note Off handler
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

bool MIDIHandler::isClockTick() {
    return MIDI.getType() == midi::Clock;
}

void MIDIHandler::clearClockTick() {
    clockTick = false;
}