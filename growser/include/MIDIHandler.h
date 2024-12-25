#ifndef MIDIHANDLER_H
#define MIDIHANDLER_H

#include <Arduino.h>
#include <MIDI.h>

#define IS_USB_CONNECTED() (usbMIDI.connected())

class MIDIHandler {
public:
    MIDIHandler();
    void begin();
    void sendControlChange(uint8_t control, uint8_t value, uint8_t channel);
    void sendNoteOn(uint8_t note, uint8_t velocity, uint8_t channel);
    void sendNoteOff(uint8_t note, uint8_t velocity, uint8_t channel);
    void processIncomingMIDI();
    bool isClockTick();
private:
    void handleControlChange(uint8_t channel, uint8_t control, uint8_t value);
    void handleNoteOn(uint8_t channel, uint8_t note, uint8_t velocity);
    void handleNoteOff(uint8_t channel, uint8_t note, uint8_t velocity);
};

#endif