// Central hub for sending and receiving MIDI.
// Relays activity to the DisplayManager and validates data.
// Created once in firmware_main.cpp.
#ifndef MIDIHANDLER_H
#define MIDIHANDLER_H

#include "Arduino.h"
#include "DisplayManager.h"

#define IS_USB_CONNECTED() (usbMidi.connected())

/**
 * @brief Thin wrapper around the Arduino and USB MIDI libraries.
 */
class MIDIHandler {
public:
    /** Assign a DisplayManager so MIDI traffic can be displayed. */
    void setDisplayManager(DisplayManager* dm) { _displayManager = dm; }

    /** Create a new MIDI handler with no side effects. */
    MIDIHandler();

    /** Set up the serial and USB MIDI interfaces. Call from setup(). */
    void begin();

    /** Send a standard Control Change message. */
    void sendControlChange(uint8_t control, uint8_t value, uint8_t channel);

    /** Send a MIDI Note On message. */
    void sendNoteOn(uint8_t note, uint8_t velocity, uint8_t channel);

    /** Send a MIDI Note Off message. */
    void sendNoteOff(uint8_t note, uint8_t velocity, uint8_t channel);

    /** Poll both serial and USB for incoming MIDI bytes. */
    void processIncomingMIDI();

    /** Dispatch a parsed MIDI message to the appropriate handler. */
    void handleMIDI(uint8_t type, uint8_t channel, uint8_t data1, uint8_t data2);

    /** Convenience helpers for specific message types. */
    void handleNoteOn(uint8_t channel, uint8_t note, uint8_t velocity);
    void handleNoteOff(uint8_t channel, uint8_t note, uint8_t velocity);
    void sendProgramChange(uint8_t program, uint8_t channel);
    void sendAftertouch(uint8_t pressure, uint8_t channel);
    void sendPitchBend(int16_t bend, uint8_t channel);
    void sendClock();

    /** MIDI clock helpers. */
    bool isClockTick();
    void clearClockTick();

private:
    bool clockTick = false;
    unsigned long lastExternalClock = 0;
    unsigned long lastInternalTick = 0;
    DisplayManager* _displayManager = nullptr;
};

#endif
