// Thin wrapper around the Teensy MIDI libraries.
// Sends and receives messages while updating DisplayManager.
// Instantiated and used throughout firmware_main.cpp.

#include "MIDIHandler.h"
#include "Globals.h"
#include "TimeUtils.h"
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
    if (g_usbMidiOutEnabled) {
        usbMIDI.sendControlChange(control, value, channel);  // USB MIDI
    }
}

void MIDIHandler::sendNoteOn(uint8_t note, uint8_t velocity, uint8_t channel) {
    if (note > 127 || velocity > 127 || channel < 1 || channel > 16)
        return;
    MIDI.sendNoteOn(note, velocity, channel);
    if (g_usbMidiOutEnabled) {
        usbMIDI.sendNoteOn(note, velocity, channel);
    }
}

void MIDIHandler::sendNoteOff(uint8_t note, uint8_t velocity, uint8_t channel) {
    if (note > 127 || velocity > 127 || channel < 1 || channel > 16)
        return;
    MIDI.sendNoteOff(note, velocity, channel);
    if (g_usbMidiOutEnabled) {
        usbMIDI.sendNoteOff(note, velocity, channel);
    }
}

void MIDIHandler::sendNRPN(uint16_t param, uint16_t value, uint8_t channel) {
    if (channel < 1 || channel > 16) return;
    if (param > 16383 || value > 16383) return;
    uint8_t pMsb = (param >> 7) & 0x7F;
    uint8_t pLsb = param & 0x7F;
    uint8_t vMsb = (value >> 7) & 0x7F;
    uint8_t vLsb = value & 0x7F;
    // classic NRPN sequence: param MSB/LSB then data entry MSB/LSB
    MIDI.sendControlChange(99, pMsb, channel);
    MIDI.sendControlChange(98, pLsb, channel);
    MIDI.sendControlChange(6,  vMsb, channel);
    MIDI.sendControlChange(38, vLsb, channel);
    if (g_usbMidiOutEnabled) {
        usbMIDI.sendControlChange(99, pMsb, channel);
        usbMIDI.sendControlChange(98, pLsb, channel);
        usbMIDI.sendControlChange(6,  vMsb, channel);
        usbMIDI.sendControlChange(38, vLsb, channel);
    }
}

void MIDIHandler::sendRPN(uint16_t param, uint16_t value, uint8_t channel) {
    if (channel < 1 || channel > 16) return;
    if (param > 16383 || value > 16383) return;
    uint8_t pMsb = (param >> 7) & 0x7F;
    uint8_t pLsb = param & 0x7F;
    uint8_t vMsb = (value >> 7) & 0x7F;
    uint8_t vLsb = value & 0x7F;
    // RPN sequence: param MSB/LSB then data entry MSB/LSB
    MIDI.sendControlChange(101, pMsb, channel);
    MIDI.sendControlChange(100, pLsb, channel);
    MIDI.sendControlChange(6,   vMsb, channel);
    MIDI.sendControlChange(38,  vLsb, channel);
    if (g_usbMidiOutEnabled) {
        usbMIDI.sendControlChange(101, pMsb, channel);
        usbMIDI.sendControlChange(100, pLsb, channel);
        usbMIDI.sendControlChange(6,   vMsb, channel);
        usbMIDI.sendControlChange(38,  vLsb, channel);
    }
}

void MIDIHandler::sendSysEx(const uint8_t* data, uint16_t length) {
    if (!data || length == 0 || length > 1024) return;
    MIDI.sendSysEx(length, data, true);
    if (g_usbMidiOutEnabled) {
        usbMIDI.sendSysEx(length, data, true);
    }
}

static bool isSupportedType(midi::MidiType t) {
    switch (t) {
        case midi::ControlChange:
        case midi::NoteOn:
        case midi::NoteOff:
        case midi::ProgramChange:
        case midi::AfterTouchChannel:
        case midi::PitchBend:
        case MidiType_SystemExclusiveStart:
        case MidiType_Tick:
            return true;
        default:
            return false;
    }
}

void MIDIHandler::handleClockTick() {
    // Flip the tick flag so listeners know a pulse went down.
    clockTick = !clockTick;
    // Mirror the tick to any enabled outputs.
    sendClock();
}

void MIDIHandler::processIncomingMIDI() {
    // Serial MIDI is the crusty hardware port. When it spits out a full
    // message, read() returns true and we hurl the parsed bytes at
    // handleMIDI so the rest of the rig can jam.
    if (MIDI.read()) {
        auto type = MIDI.getType();
        if (type == MidiType_Tick) {
            lastExternalClock = lastInternalTick = now();
            handleClockTick();
        } else if (type == MidiType_SystemExclusiveStart) {
            handleSysEx(MIDI.getSysExArray(), MIDI.getSysExArrayLength());
        } else {
            handleMIDI(type, MIDI.getChannel(), MIDI.getData1(), MIDI.getData2());
        }
    }

    // USB MIDI stockpiles packets in a buffer. Drain that queue in a loop
    // so nothing gets stale, feeding each packet through the same handler as
    // the old-school wire.
    while (usbMIDI.read()) {
        // Force usbMIDI's raw type into midi::MidiType so isSupportedType doesn't choke
        auto type = static_cast<midi::MidiType>(usbMIDI.getType());
        if (!isSupportedType(type)) {
            MIDI_DBG_PRINTLN("Dropping unsupported USB MIDI type");
            continue;
        }
        if (type == MidiType_Tick) {
            lastExternalClock = lastInternalTick = now();
            handleClockTick();
        } else if (type == MidiType_SystemExclusiveStart) {
            handleSysEx(usbMIDI.getSysExArray(), usbMIDI.getSysExArrayLength());
        } else {
            handleMIDI(type, usbMIDI.getChannel(), usbMIDI.getData1(), usbMIDI.getData2());
        }
    }

    // If the outside world goes quiet, puke out our own clock based on tapped BPM
    if (g_tappedBPM > 0.0f) {
        bool externalHot = (now() - lastExternalClock) < CLOCK_TIMEOUT_MS;
        if (!externalHot) {
            float msPerTick = 60000.0f / (g_tappedBPM * 24.0f);
            if (now() - lastInternalTick >= msPerTick) {
                lastInternalTick = now();
                handleClockTick();
            }
        }
    }

    if (_displayManager) {
        _displayManager->registerInteraction();
    }
}

void MIDIHandler::handleMIDI(midi::MidiType type, uint8_t channel, uint8_t data1, uint8_t data2) {
    if (channel < 1 || channel > 16 || data1 > 127 || data2 > 127) {
        MIDI_DBG_PRINTLN("Bad MIDI data, dropped");
        return;
    }
    switch (type) {
        case midi::ControlChange:
            // Peek for NRPN sequences; otherwise just log the CC
            switch (data1) {
                case 101: // RPN parameter MSB
                    _rpnParam = (data2 & 0x7F) << 7;
                    _rpnParamReady = false;
                    break;
                case 100: // RPN parameter LSB
                    _rpnParam |= (data2 & 0x7F);
                    _rpnParamReady = true;
                    break;
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
                    _rpnValue  = (data2 & 0x7F) << 7;
                    break;
                case 38: // Data entry LSB
                    _nrpnValue |= (data2 & 0x7F);
                    _rpnValue  |= (data2 & 0x7F);
                    if (_nrpnParamReady) {
                        receiveNRPN(channel, _nrpnParam, _nrpnValue);
                        _nrpnParamReady = false;
                    }
                    if (_rpnParamReady) {
                        receiveRPN(channel, _rpnParam, _rpnValue);
                        _rpnParamReady = false;
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
        case midi::ProgramChange:
            handleProgramChange(channel, data1);
            break;
        case midi::AfterTouchChannel:
            handleAftertouch(channel, data1);
            break;
        case midi::PitchBend: {
            int16_t bend = ((data2 & 0x7F) << 7) | (data1 & 0x7F);
            bend -= 8192;
            handlePitchBend(channel, bend);
            break;
        }
        case MidiType_SystemExclusiveStart:
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
    if (g_usbMidiOutEnabled) {
        usbMIDI.sendNoteOn(note, velocity, channel);
    }
}

void MIDIHandler::handleNoteOff(uint8_t channel, uint8_t note, uint8_t velocity) {
    MIDI_DBG_PRINTF("Note Off: %d, Velocity: %d, Channel: %d\n", note, velocity, channel);
    MIDI.sendNoteOff(note, velocity, channel);
    if (g_usbMidiOutEnabled) {
        usbMIDI.sendNoteOff(note, velocity, channel);
    }
}

void MIDIHandler::handleProgramChange(uint8_t channel, uint8_t program) {
    MIDI_DBG_PRINTF("Program Change: %d, Channel: %d\n", program, channel);
    sendProgramChange(program, channel);
}

void MIDIHandler::handleAftertouch(uint8_t channel, uint8_t pressure) {
    MIDI_DBG_PRINTF("Aftertouch: %d, Channel: %d\n", pressure, channel);
    sendAftertouch(pressure, channel);
}

void MIDIHandler::handlePitchBend(uint8_t channel, int16_t bend) {
    MIDI_DBG_PRINTF("Pitch Bend: %d, Channel: %d\n", bend, channel);
    sendPitchBend(bend, channel);
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
  if (g_usbMidiOutEnabled) {
    usbMIDI.sendProgramChange(program, channel);
  }
}

void MIDIHandler::sendAftertouch(uint8_t pressure, uint8_t channel) {
  if (pressure>127|| channel<1||channel>16) return;
  MIDI.sendAfterTouch(pressure, channel);
  if (g_usbMidiOutEnabled) {
    usbMIDI.sendAfterTouch(pressure, channel);
  }
}

void MIDIHandler::sendPitchBend(int16_t bend, uint8_t channel) {
  // Validate channel and clamp bend value
  if (channel < 1 || channel > 16) return;
  if (bend < -8192) bend = -8192;
  else if (bend > 8191) bend = 8191;

  // Teensy and USB MIDI libraries accept the signed 14-bit value directly
  MIDI.sendPitchBend(bend, channel);
  if (g_usbMidiOutEnabled) {
    usbMIDI.sendPitchBend(bend, channel);
  }
}

void MIDIHandler::sendClock() {
  if (!g_clockOutEnabled) return;
  MIDI.sendClock();
  if (g_usbMidiOutEnabled) {
    usbMIDI.sendClock();
  }
}

void MIDIHandler::receiveNRPN(uint8_t channel, uint16_t param, uint16_t value) {
    _lastNRPNParam = param;
    _lastNRPNValue = value;
    MIDI_DBG_PRINTF("NRPN %u = %u on ch %u\n", param, value, channel);
}

void MIDIHandler::receiveRPN(uint8_t channel, uint16_t param, uint16_t value) {
    _lastRPNParam = param;
    _lastRPNValue = value;
    MIDI_DBG_PRINTF("RPN %u = %u on ch %u\n", param, value, channel);
}

void MIDIHandler::handleSysEx(const uint8_t* data, uint16_t length) {
    if (!data || length == 0) return;
    _lastSysExLength = (length > sizeof(_lastSysEx)) ? sizeof(_lastSysEx) : length;
    for (uint16_t i = 0; i < _lastSysExLength; ++i) {
        _lastSysEx[i] = data[i];
    }
    _lastSysExType = SysExType::ManufacturerSpecific;
    _lastSysExSubId1 = _lastSysExSubId2 = 0;
    if (length >= 5) {
        uint8_t manufacturer = data[1];
        if (manufacturer == 0x7E || manufacturer == 0x7F) {
            _lastSysExType = (manufacturer == 0x7E) ?
                SysExType::UniversalNonRealTime : SysExType::UniversalRealTime;
            _lastSysExSubId1 = data[3];
            _lastSysExSubId2 = data[4];
        }
    }
    MIDI_DBG_PRINTF("SysEx[%u]", length);
    for (uint16_t i = 0; i < length; ++i) {
        MIDI_DBG_PRINTF(" %02X", data[i]);
    }
    MIDI_DBG_PRINTLN("");
}
