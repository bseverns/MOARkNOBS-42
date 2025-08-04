// Thin wrapper around the Teensy MIDI libraries.
// Sends and receives messages while updating DisplayManager.
// Instantiated and used throughout firmware_main.cpp.

#include "MIDIHandler.h"
#include "Globals.h"
#include <USB-MIDI.h>

// Serial debug wrappers. Flip `MIDI_DEBUG` at build time to spew or silence.
#ifdef MIDI_DEBUG
  #define MIDI_DBG_PRINTF(...) Serial.printf(__VA_ARGS__)
  #define MIDI_DBG_PRINTLN(x) Serial.println(x)
#else
  #define MIDI_DBG_PRINTF(...)
  #define MIDI_DBG_PRINTLN(x)
#endif

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

void MIDIHandler::sendNRPN(uint16_t param, uint16_t value, uint8_t channel) {
    if (channel < 1 || channel > 16) return;
    uint8_t pMsb = (param >> 7) & 0x7F;
    uint8_t pLsb = param & 0x7F;
    uint8_t vMsb = (value >> 7) & 0x7F;
    uint8_t vLsb = value & 0x7F;
    // classic NRPN sequence: param MSB/LSB then data entry MSB/LSB
    MIDI.sendControlChange(99, pMsb, channel);
    MIDI.sendControlChange(98, pLsb, channel);
    MIDI.sendControlChange(6,  vMsb, channel);
    MIDI.sendControlChange(38, vLsb, channel);
    usbMIDI.sendControlChange(99, pMsb, channel);
    usbMIDI.sendControlChange(98, pLsb, channel);
    usbMIDI.sendControlChange(6,  vMsb, channel);
    usbMIDI.sendControlChange(38, vLsb, channel);
}

void MIDIHandler::sendSysEx(const uint8_t* data, uint16_t length) {
    if (!data || length == 0) return;
    MIDI.sendSysEx(length, data, true);
    usbMIDI.sendSysEx(length, data, true);
}

void MIDIHandler::processIncomingMIDI() {
    // Serial MIDI is the crusty hardware port. When it spits out a full
    // message, read() returns true and we hurl the parsed bytes at
    // handleMIDI so the rest of the rig can jam.
    if (MIDI.read()) {
        auto type = MIDI.getType();
        if (type == midi::Clock) {
            clockTick = true;
            lastExternalClock = lastInternalTick = millis();
            if (g_clockOutEnabled) {
                MIDI.sendClock();
                usbMIDI.sendClock();
            }
        } else if (type == midi::SystemExclusive) {
            handleSysEx(MIDI.getSysExArray(), MIDI.getSysExArrayLength());
        } else {
            handleMIDI(type, MIDI.getChannel(), MIDI.getData1(), MIDI.getData2());
        }
    }

    // USB MIDI stockpiles packets in a buffer. Drain that queue in a loop
    // so nothing gets stale, feeding each packet through the same handler as
    // the old-school wire.
    while (usbMIDI.read()) {
        auto type = usbMIDI.getType();
        if (type == midi::Clock) {
            clockTick = true;
            lastExternalClock = lastInternalTick = millis();
            if (g_clockOutEnabled) {
                MIDI.sendClock();
                usbMIDI.sendClock();
            }
        } else if (type == midi::SystemExclusive) {
            handleSysEx(usbMIDI.getSysExArray(), usbMIDI.getSysExArrayLength());
        } else {
            handleMIDI(type, usbMIDI.getChannel(), usbMIDI.getData1(), usbMIDI.getData2());
        }
    }

    // If the outside world goes quiet, puke out our own clock based on tapped BPM
    if (g_tappedBPM > 0.0f) {
        bool externalHot = (millis() - lastExternalClock) < CLOCK_TIMEOUT_MS;
        if (!externalHot) {
            float msPerTick = 60000.0f / (g_tappedBPM * 24.0f);
            if (millis() - lastInternalTick >= msPerTick) {
                lastInternalTick = millis();
                if (g_clockOutEnabled) {
                    MIDI.sendClock();
                    usbMIDI.sendClock();
                }
                clockTick = true;
            }
        }
    }

    if (_displayManager) {
        _displayManager->registerInteraction();
    }
}

void MIDIHandler::handleMIDI(uint8_t type, uint8_t channel, uint8_t data1, uint8_t data2) {
    switch (type) {
        case midi::ControlChange:
            // Peek for NRPN sequences; otherwise just log the CC
            switch (data1) {
                case 99: // NRPN parameter MSB
                    _nrpnParam = (data2 & 0x7F) << 7;
                    _nrpnParamReady = false;
                    break;
                case 98: // NRPN parameter LSB
                    _nrpnParam |= (data2 & 0x7F);
                    _nrpnParamReady = true;
                    break;
                case 6: // Data entry MSB
                    _nrpnValue = (data2 & 0x7F) << 7;
                    break;
                case 38: // Data entry LSB
                    _nrpnValue |= (data2 & 0x7F);
                    if (_nrpnParamReady) {
                        handleNRPN(channel, _nrpnParam, _nrpnValue);
                    }
                    break;
                default:
                    MIDI_DBG_PRINTF("CC: %d, Value: %d, Channel: %d\n", data1, data2, channel);
                    break;
            }
            break;
        case midi::NoteOn:
            handleNoteOn(channel, data1, data2);
            break;
        case midi::NoteOff:
            handleNoteOff(channel, data1, data2);
            break;
        case midi::SystemExclusive:
            // handled upstream
            break;
        default:
            MIDI_DBG_PRINTLN("Unhandled MIDI message");
            break;
    }
}

void MIDIHandler::handleNoteOn(uint8_t channel, uint8_t note, uint8_t velocity) {
    MIDI_DBG_PRINTF("Note On: %d, Velocity: %d, Channel: %d\n", note, velocity, channel);
    MIDI.sendNoteOn(note, velocity, channel);
    usbMIDI.sendNoteOn(note, velocity, channel);
}

void MIDIHandler::handleNoteOff(uint8_t channel, uint8_t note, uint8_t velocity) {
    MIDI_DBG_PRINTF("Note Off: %d, Velocity: %d, Channel: %d\n", note, velocity, channel);
    MIDI.sendNoteOff(note, velocity, channel);
    usbMIDI.sendNoteOff(note, velocity, channel);
}

// Did we just spew or hear a MIDI clock pulse since last check?
bool MIDIHandler::isClockTick() {
    return clockTick;
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

void MIDIHandler::sendClock() {
  if (!g_clockOutEnabled) return;
  MIDI.sendClock();
  usbMIDI.sendClock();
}

void MIDIHandler::handleNRPN(uint8_t channel, uint16_t param, uint16_t value) {
    MIDI_DBG_PRINTF("NRPN %u = %u on ch %u\n", param, value, channel);
}

void MIDIHandler::handleSysEx(const uint8_t* data, uint16_t length) {
    MIDI_DBG_PRINTF("SysEx[%u]", length);
    for (uint16_t i = 0; i < length; ++i) {
        MIDI_DBG_PRINTF(" %02X", data[i]);
    }
    MIDI_DBG_PRINTLN("");
}
