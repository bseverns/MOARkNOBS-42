#ifndef MIDIHANDLER_H
#define MIDIHANDLER_H

#include <Arduino.h>
#include <MIDI.h>
#include <SequenceManager.h>

class Sequencer;
class MIDIHandler {
public:
    MIDIHandler();
    void begin();
    void sendControlChange(uint8_t control, uint8_t value, uint8_t channel);
    void sendNoteOn(uint8_t note, uint8_t velocity, uint8_t channel);
    void sendNoteOff(uint8_t note, uint8_t velocity, uint8_t channel);
    void processIncomingMIDI();
    bool isClockTick();
    void handleClockMessage(Sequencer &sequencer); // Declare the method here
private:
    void handleControlChange(uint8_t channel, uint8_t control, uint8_t value);
    void handleNoteOn(uint8_t channel, uint8_t note, uint8_t velocity);
    void handleNoteOff(uint8_t channel, uint8_t note, uint8_t velocity);
};

#endif