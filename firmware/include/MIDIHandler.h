#ifndef MIDIHANDLER_H
#define MIDIHANDLER_H

#include "Arduino.h"
//#include "MIDI.h"
#include "DisplayManager.h"

#define IS_USB_CONNECTED() (usbMIDI.connected())

class MIDIHandler {
public:
    void setDisplayManager(DisplayManager* dm) { _displayManager = dm; }
    MIDIHandler();
    void begin();
    void sendControlChange(uint8_t control, uint8_t value, uint8_t channel);
    void sendNoteOn(uint8_t note, uint8_t velocity, uint8_t channel);
    void sendNoteOff(uint8_t note, uint8_t velocity, uint8_t channel);
    void processIncomingMIDI();
    void handleMIDI(uint8_t type, uint8_t channel, uint8_t data1, uint8_t data2); // Process a generic MIDI message
    void handleNoteOn(uint8_t channel, uint8_t note, uint8_t velocity);
    void handleNoteOff(uint8_t channel, uint8_t note, uint8_t velocity);
    bool isClockTick();
    void clearClockTick();

private:
    bool clockTick = false;
    DisplayManager* _displayManager = nullptr;
};

#endif
