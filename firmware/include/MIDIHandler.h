// Central hub for sending and receiving MIDI.
// Relays activity to the DisplayManager and validates data.
// Created once in firmware_main.cpp.
#ifndef MIDIHANDLER_H
#define MIDIHANDLER_H

#ifdef ARDUINO
#include "Arduino.h"
#else
class HardwareSerial;
#endif
#include "DisplayManager.h"
#include "MIDITypes.h"

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

    /** Sling a raw NRPN sequence (CC99/98 + CC6/38). */
    void sendNRPN(uint16_t param, uint16_t value, uint8_t channel);

    /** Sling a Registered Parameter Number sequence (CC101/100 + CC6/38). */
    void sendRPN(uint16_t param, uint16_t value, uint8_t channel);

    /** Last NRPN parsed from the wire. */
    uint16_t lastNRPNParam() const { return _lastNRPNParam; }
    uint16_t lastNRPNValue() const { return _lastNRPNValue; }

    /** Last RPN parsed from the wire. */
    uint16_t lastRPNParam() const { return _lastRPNParam; }
    uint16_t lastRPNValue() const { return _lastRPNValue; }

    /** Fire off a System Exclusive packet. `data` should include F0/F7. */
    void sendSysEx(const uint8_t* data, uint16_t length);

    /** Snapshot of the most recent SysEx payload. */
    uint16_t lastSysExLength() const { return _lastSysExLength; }
    const uint8_t* lastSysExData() const { return _lastSysEx; }
    SysExType lastSysExType() const { return _lastSysExType; }
    uint8_t lastSysExSubId1() const { return _lastSysExSubId1; }
    uint8_t lastSysExSubId2() const { return _lastSysExSubId2; }

    /** Poll both serial and USB for incoming MIDI bytes. */
    void processIncomingMIDI();

    /** Dispatch a parsed MIDI message to the appropriate handler. */
    void handleMIDI(uint8_t type, uint8_t channel, uint8_t data1, uint8_t data2);

    /** Convenience helpers for specific message types. */
    void handleNoteOn(uint8_t channel, uint8_t note, uint8_t velocity);
    void handleNoteOff(uint8_t channel, uint8_t note, uint8_t velocity);
    void handleProgramChange(uint8_t channel, uint8_t program);
    void handleAftertouch(uint8_t channel, uint8_t pressure);
    void handlePitchBend(uint8_t channel, int16_t bend);
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

    // NRPN decode state
    uint16_t _nrpnParam = 0;
    uint16_t _nrpnValue = 0;
    bool     _nrpnParamReady = false;

    // Last fully received NRPN for external inspection
    uint16_t _lastNRPNParam = 0;
    uint16_t _lastNRPNValue = 0;

    // RPN decode state
    uint16_t _rpnParam = 0;
    uint16_t _rpnValue = 0;
    bool     _rpnParamReady = false;

    // Last fully received RPN for external inspection
    uint16_t _lastRPNParam = 0;
    uint16_t _lastRPNValue = 0;

    // SysEx stash for quick testing/debugging
    uint8_t  _lastSysEx[32] = {0};
    uint16_t _lastSysExLength = 0;
    SysExType _lastSysExType = SysExType::ManufacturerSpecific;
    uint8_t _lastSysExSubId1 = 0;
    uint8_t _lastSysExSubId2 = 0;

    void receiveNRPN(uint8_t channel, uint16_t param, uint16_t value);
    void receiveRPN(uint8_t channel, uint16_t param, uint16_t value);
    void handleSysEx(const uint8_t* data, uint16_t length);
};

#endif
