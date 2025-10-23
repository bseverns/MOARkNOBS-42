// Thin wrapper around the Teensy MIDI libraries.
// Sends and receives messages while updating DisplayManager.
// Instantiated and used throughout firmware_main.cpp.

#include "MIDIHandler.h"
#include "Globals.h"
#include "TimeUtils.h"
#include "Log.h"
#include "interop/SeedBoxLink.h"
#include "interop/mn42_map.h"
#include "version.h"

#include <array>

// Serial debug wrappers. Flip `MIDI_DEBUG` at build time to spew or silence.
#ifdef MIDI_DEBUG
#define MIDI_DBG_PRINTF(...) LOG_PRINTF(__VA_ARGS__)
#define MIDI_DBG_PRINTLN(...) LOG_PRINTLN(__VA_ARGS__)
#else
#define MIDI_DBG_PRINTF(...)
#define MIDI_DBG_PRINTLN(...)
#endif

// Provides a small abstraction over both Serial and USB MIDI transports. Other
// modules call these helpers to send messages, while incoming data is routed to
// callbacks that update display and arpeggiator state.

#ifndef USB_MIDI_STUB
MIDI_CREATE_INSTANCE(HardwareSerial, Serial1, MIDI);
#endif

MIDIHandler::MIDIHandler() {}

void MIDIHandler::begin() {
    MIDI.begin(); // default Omni channel; stub libs may skip the constant
#ifndef USB_MIDI_STUB
    usbMIDI.begin();
#endif
}

void MIDIHandler::sendControlChange(uint8_t control, uint8_t value, uint8_t channel) {
    // Validate before sending
    if (control > 127 || value > 127 || channel < 1 || channel > 16)
        return;
    _txCount++;
    enqueueSerialMessage(makeControlChange(channel, control, value));
    serviceSerialQueue();
#ifndef USB_MIDI_STUB
    if (g_usbMidiOutEnabled) {
        usbMIDI.sendControlChange(control, value, channel); // USB MIDI
    }
#endif
}

void MIDIHandler::sendNoteOn(uint8_t note, uint8_t velocity, uint8_t channel) {
    if (note > 127 || velocity > 127 || channel < 1 || channel > 16)
        return;
    _txCount++;
    enqueueSerialMessage(makeNoteOn(channel, note, velocity));
    serviceSerialQueue();
#ifndef USB_MIDI_STUB
    if (g_usbMidiOutEnabled) {
        usbMIDI.sendNoteOn(note, velocity, channel);
    }
#endif
}

void MIDIHandler::sendNoteOff(uint8_t note, uint8_t velocity, uint8_t channel) {
    if (note > 127 || velocity > 127 || channel < 1 || channel > 16)
        return;
    _txCount++;
    enqueueSerialMessage(makeNoteOff(channel, note, velocity));
    serviceSerialQueue();
#ifndef USB_MIDI_STUB
    if (g_usbMidiOutEnabled) {
        usbMIDI.sendNoteOff(note, velocity, channel);
    }
#endif
}

void MIDIHandler::sendNRPN(uint16_t param, uint16_t value, uint8_t channel) {
    if (channel < 1 || channel > 16)
        return;
    if (param > 16383 || value > 16383)
        return;
    _txCount++;
    uint8_t pMsb = (param >> 7) & 0x7F;
    uint8_t pLsb = param & 0x7F;
    uint8_t vMsb = (value >> 7) & 0x7F;
    uint8_t vLsb = value & 0x7F;
    // classic NRPN sequence: param MSB/LSB then data entry MSB/LSB
    enqueueSerialMessage(makeControlChange(channel, 99, pMsb));
    enqueueSerialMessage(makeControlChange(channel, 98, pLsb));
    enqueueSerialMessage(makeControlChange(channel, 6, vMsb));
    enqueueSerialMessage(makeControlChange(channel, 38, vLsb));
    serviceSerialQueue();
#ifndef USB_MIDI_STUB
    if (g_usbMidiOutEnabled) {
        usbMIDI.sendControlChange(99, pMsb, channel);
        usbMIDI.sendControlChange(98, pLsb, channel);
        usbMIDI.sendControlChange(6, vMsb, channel);
        usbMIDI.sendControlChange(38, vLsb, channel);
    }
#endif
}

void MIDIHandler::sendRPN(uint16_t param, uint16_t value, uint8_t channel) {
    if (channel < 1 || channel > 16)
        return;
    if (param > 16383 || value > 16383)
        return;
    _txCount++;
    uint8_t pMsb = (param >> 7) & 0x7F;
    uint8_t pLsb = param & 0x7F;
    uint8_t vMsb = (value >> 7) & 0x7F;
    uint8_t vLsb = value & 0x7F;
    // RPN sequence: param MSB/LSB then data entry MSB/LSB
    enqueueSerialMessage(makeControlChange(channel, 101, pMsb));
    enqueueSerialMessage(makeControlChange(channel, 100, pLsb));
    enqueueSerialMessage(makeControlChange(channel, 6, vMsb));
    enqueueSerialMessage(makeControlChange(channel, 38, vLsb));
    serviceSerialQueue();
#ifndef USB_MIDI_STUB
    if (g_usbMidiOutEnabled) {
        usbMIDI.sendControlChange(101, pMsb, channel);
        usbMIDI.sendControlChange(100, pLsb, channel);
        usbMIDI.sendControlChange(6, vMsb, channel);
        usbMIDI.sendControlChange(38, vLsb, channel);
    }
#endif
}

void MIDIHandler::sendSysEx(const uint8_t *data, uint16_t length) {
    if (!data || length == 0 || length > 1024)
        return;
    _txCount++;
    MIDI.sendSysEx(length, data, true);
#ifndef USB_MIDI_STUB
    if (g_usbMidiOutEnabled) {
        usbMIDI.sendSysEx(length, data, true);
    }
#endif
}

// Utility to filter USB MIDI packets we actually care about. Tagged
// [[maybe_unused]] because USB builds might stub out the call site.
[[maybe_unused]] static bool isSupportedType(midi::MidiType t) {
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
    // Flag that a new pulse landed so every listener can catch the beat.
    clockTick = true;
    ++_clockTickCount;
    // Mirror the tick to any enabled outputs.
    sendClock();
}

void MIDIHandler::processIncomingMIDI() {
    // Even if no new traffic lands this frame, keep draining the DIN queue so
    // anything throttled by the pacing guard still hits the wire ASAP.
    serviceSerialQueue();

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
#ifndef USB_MIDI_STUB
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
#endif

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

    serviceSerialQueue();
}

void MIDIHandler::handleMIDI(midi::MidiType type, uint8_t channel, uint8_t data1, uint8_t data2) {
    if (channel < 1 || channel > 16 || data1 > 127 || data2 > 127) {
        MIDI_DBG_PRINTLN("Bad MIDI data, dropped");
        return;
    }
    _rxCount++;
    switch (type) {
    case midi::ControlChange:
        // Peek for NRPN sequences; otherwise just log the CC
        if (seedbox::interop::mn42::SeedBoxLink::instance().handleControlChange(channel, data1,
                                                                                data2)) {
            break;
        }
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
            _rpnValue = (data2 & 0x7F) << 7;
            break;
        case 38: // Data entry LSB
            _nrpnValue |= (data2 & 0x7F);
            _rpnValue |= (data2 & 0x7F);
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
    enqueueSerialMessage(makeNoteOn(channel, note, velocity));
    serviceSerialQueue();
#ifndef USB_MIDI_STUB
    if (g_usbMidiOutEnabled) {
        usbMIDI.sendNoteOn(note, velocity, channel);
    }
#endif
}

void MIDIHandler::handleNoteOff(uint8_t channel, uint8_t note, uint8_t velocity) {
    MIDI_DBG_PRINTF("Note Off: %d, Velocity: %d, Channel: %d\n", note, velocity, channel);
    enqueueSerialMessage(makeNoteOff(channel, note, velocity));
    serviceSerialQueue();
#ifndef USB_MIDI_STUB
    if (g_usbMidiOutEnabled) {
        usbMIDI.sendNoteOff(note, velocity, channel);
    }
#endif
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
bool MIDIHandler::isClockTick() { return clockTick; }

// Wipe the clock pulse flag so the next beat counts.
void MIDIHandler::clearClockTick() { clockTick = false; }

void MIDIHandler::sendProgramChange(uint8_t program, uint8_t channel) {
    if (program > 127 || channel < 1 || channel > 16)
        return;
    _txCount++;
    enqueueSerialMessage(makeProgramChange(channel, program));
    serviceSerialQueue();
#ifndef USB_MIDI_STUB
    if (g_usbMidiOutEnabled) {
        usbMIDI.sendProgramChange(program, channel);
    }
#endif
}

void MIDIHandler::sendAftertouch(uint8_t pressure, uint8_t channel) {
    if (pressure > 127 || channel < 1 || channel > 16)
        return;
    enqueueSerialMessage(makeAftertouch(channel, pressure));
    serviceSerialQueue();
#ifndef USB_MIDI_STUB
    if (g_usbMidiOutEnabled) {
        usbMIDI.sendAfterTouch(pressure, channel);
    }
#endif
    _txCount++;
}

void MIDIHandler::sendModWheel(uint8_t value, uint8_t channel) {
    sendControlChange(1, value, channel);
}

void MIDIHandler::sendPitchBend(int16_t bend, uint8_t channel) {
    // Validate channel and clamp bend value
    if (channel < 1 || channel > 16)
        return;
    if (bend < -8192)
        bend = -8192;
    else if (bend > 8191)
        bend = 8191;

    // Teensy and USB MIDI libraries accept the signed 14-bit value directly
    enqueueSerialMessage(makePitchBend(channel, bend));
    serviceSerialQueue();
#ifndef USB_MIDI_STUB
    if (g_usbMidiOutEnabled) {
        usbMIDI.sendPitchBend(bend, channel);
    }
#endif
    _txCount++;
}

void MIDIHandler::sendClock() {
    if (!g_clockOutEnabled)
        return;
    enqueueSerialMessage(makeClock());
    serviceSerialQueue();
#ifndef USB_MIDI_STUB
    if (g_usbMidiOutEnabled) {
        usbMIDI.sendClock();
    }
#endif
    _txCount++;
}

bool MIDIHandler::enqueueSerialMessage(const SerialMessage &msg) {
    if (msg.byteCount == 0) {
        return false;
    }

    if (_serialQueueFull) {
        return tryCoalesceSerialMessage(msg);
    }

    if (serialQueueSize() >= kSerialQueueSize - 4 && tryCoalesceSerialMessage(msg)) {
        return true;
    }

    _serialQueue[_serialQueueTail] = msg;
    _serialQueueTail = (_serialQueueTail + 1) % kSerialQueueSize;
    if (_serialQueueTail == _serialQueueHead) {
        _serialQueueFull = true;
    }
    return true;
}

bool MIDIHandler::dequeueSerialMessage(SerialMessage &msg) {
    if (serialQueueEmpty()) {
        return false;
    }
    msg = _serialQueue[_serialQueueHead];
    _serialQueueHead = (_serialQueueHead + 1) % kSerialQueueSize;
    _serialQueueFull = false;
    return true;
}

bool MIDIHandler::serialQueueEmpty() const {
    return !_serialQueueFull && (_serialQueueHead == _serialQueueTail);
}

size_t MIDIHandler::serialQueueSize() const {
    if (_serialQueueFull) {
        return kSerialQueueSize;
    }
    if (_serialQueueTail >= _serialQueueHead) {
        return _serialQueueTail - _serialQueueHead;
    }
    return kSerialQueueSize - (_serialQueueHead - _serialQueueTail);
}

bool MIDIHandler::tryCoalesceSerialMessage(const SerialMessage &msg) {
    auto shouldCoalesce = [&](const SerialMessage &candidate) {
        if (candidate.type != msg.type) {
            return false;
        }
        switch (msg.type) {
        case SerialMessageType::ControlChange:
            return candidate.channel == msg.channel && candidate.data1 == msg.data1;
        case SerialMessageType::Aftertouch:
            return candidate.channel == msg.channel;
        case SerialMessageType::PitchBend:
            return candidate.channel == msg.channel;
        default:
            return false;
        }
    };

    const size_t count = serialQueueSize();
    if (count == 0) {
        return false;
    }

    std::array<SerialMessage, kSerialQueueSize> compact{};
    size_t compactCount = 0;
    bool foundMatch = false;
    for (size_t i = 0; i < count; ++i) {
        size_t index = (_serialQueueHead + i) % kSerialQueueSize;
        SerialMessage candidate = _serialQueue[index];
        if (shouldCoalesce(candidate)) {
            foundMatch = true;
            // Every duplicate gets axed so we can tack the fresh value onto the tail.
            continue;
        }
        compact[compactCount++] = candidate;
    }

    if (!foundMatch) {
        return false;
    }

    if (compactCount < kSerialQueueSize) {
        compact[compactCount++] = msg;
    } else {
        // Should be unreachable because we dropped at least one entry, but fail safe.
        compact[kSerialQueueSize - 1] = msg;
    }

    for (size_t i = 0; i < compactCount; ++i) {
        _serialQueue[i] = compact[i];
    }
    _serialQueueHead = 0;
    _serialQueueTail = compactCount % kSerialQueueSize;
    _serialQueueFull = (compactCount == kSerialQueueSize);
    return true;
}

void MIDIHandler::serviceSerialQueue() {
#ifdef USB_MIDI_STUB
    SerialMessage msg;
    while (dequeueSerialMessage(msg)) {
        dispatchSerialMessage(msg);
    }
#else
    while (!serialQueueEmpty()) {
        const SerialMessage &next = _serialQueue[_serialQueueHead];
        uint32_t required = static_cast<uint32_t>(next.byteCount) * kSerialByteMicros;
        uint32_t nowUs = micros();
        uint32_t elapsed = nowUs - _lastSerialSendUs;
        if (elapsed < required) {
            break;
        }
        SerialMessage msg;
        if (!dequeueSerialMessage(msg)) {
            break;
        }
        dispatchSerialMessage(msg);
        _lastSerialSendUs = nowUs;
    }
#endif
}

void MIDIHandler::dispatchSerialMessage(const SerialMessage &msg) {
    switch (msg.type) {
    case SerialMessageType::ControlChange:
        MIDI.sendControlChange(msg.data1, msg.data2, msg.channel);
        break;
    case SerialMessageType::NoteOn:
        MIDI.sendNoteOn(msg.data1, msg.data2, msg.channel);
        break;
    case SerialMessageType::NoteOff:
        MIDI.sendNoteOff(msg.data1, msg.data2, msg.channel);
        break;
    case SerialMessageType::ProgramChange:
        MIDI.sendProgramChange(msg.data1, msg.channel);
        break;
    case SerialMessageType::Aftertouch:
        MIDI.sendAfterTouch(msg.data1, msg.channel);
        break;
    case SerialMessageType::PitchBend:
        MIDI.sendPitchBend(msg.pitch, msg.channel);
        break;
    case SerialMessageType::Clock:
        MIDI.sendClock();
        break;
    }
}

MIDIHandler::SerialMessage MIDIHandler::makeControlChange(uint8_t channel, uint8_t control,
                                                          uint8_t value) const {
    SerialMessage msg;
    msg.type = SerialMessageType::ControlChange;
    msg.channel = channel;
    msg.data1 = control;
    msg.data2 = value;
    msg.byteCount = 3;
    return msg;
}

MIDIHandler::SerialMessage MIDIHandler::makeNoteOn(uint8_t channel, uint8_t note,
                                                   uint8_t velocity) const {
    SerialMessage msg;
    msg.type = SerialMessageType::NoteOn;
    msg.channel = channel;
    msg.data1 = note;
    msg.data2 = velocity;
    msg.byteCount = 3;
    return msg;
}

MIDIHandler::SerialMessage MIDIHandler::makeNoteOff(uint8_t channel, uint8_t note,
                                                    uint8_t velocity) const {
    SerialMessage msg;
    msg.type = SerialMessageType::NoteOff;
    msg.channel = channel;
    msg.data1 = note;
    msg.data2 = velocity;
    msg.byteCount = 3;
    return msg;
}

MIDIHandler::SerialMessage MIDIHandler::makeProgramChange(uint8_t channel, uint8_t program) const {
    SerialMessage msg;
    msg.type = SerialMessageType::ProgramChange;
    msg.channel = channel;
    msg.data1 = program;
    msg.byteCount = 2;
    return msg;
}

MIDIHandler::SerialMessage MIDIHandler::makeAftertouch(uint8_t channel, uint8_t pressure) const {
    SerialMessage msg;
    msg.type = SerialMessageType::Aftertouch;
    msg.channel = channel;
    msg.data1 = pressure;
    msg.byteCount = 2;
    return msg;
}

MIDIHandler::SerialMessage MIDIHandler::makePitchBend(uint8_t channel, int16_t bend) const {
    SerialMessage msg;
    msg.type = SerialMessageType::PitchBend;
    msg.channel = channel;
    msg.pitch = bend;
    msg.byteCount = 3;
    return msg;
}

MIDIHandler::SerialMessage MIDIHandler::makeClock() const {
    SerialMessage msg;
    msg.type = SerialMessageType::Clock;
    msg.byteCount = 1;
    return msg;
}

void MIDIHandler::generateClockTick() {
    lastInternalTick = now();
    handleClockTick();
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

void MIDIHandler::handleSysEx(const uint8_t *data, uint16_t length) {
    if (!data || length < 2) {
#ifdef MIDI_DEBUG
        MIDI_DBG_PRINTF("Dropping SysEx with length %u (too short for framing)\n", length);
#endif
        return;
    }

    if (data[0] != 0xF0 || data[length - 1] != 0xF7) {
#ifdef MIDI_DEBUG
        MIDI_DBG_PRINTF("Dropping SysEx[%u]: invalid framing (start %02X end %02X)\n", length,
                        data[0], data[length - 1]);
#endif
        return;
    }

    _rxCount++;
    seedbox::interop::mn42::SeedBoxLink::instance().handleSysEx(data, length);
    bool isIdentityRequest = length >= 6 && data[0] == 0xF0 && data[length - 1] == 0xF7 &&
                             data[1] == 0x7E && data[3] == 0x06 && data[4] == 0x01;
    if (isIdentityRequest) {
        constexpr size_t kIdentityReplyLength = 15;
        std::array<uint8_t, kIdentityReplyLength> reply{};
        size_t idx = 0;
        auto push = [&](uint8_t value) {
            if (idx < reply.size()) {
                reply[idx++] = value;
            }
        };
        const uint8_t deviceId = data[2];
        push(0xF0);
        push(0x7E);
        push(deviceId);
        push(0x06);
        push(0x02);
        constexpr std::array<uint8_t, 1> manufacturer = {
            seedbox::interop::mn42::handshake::product::kManufacturerId};
        constexpr std::array<uint8_t, 2> family = {
            seedbox::interop::mn42::handshake::product::kSignature0,
            seedbox::interop::mn42::handshake::product::kSignature1};
        constexpr std::array<uint8_t, 2> model = {
            seedbox::interop::mn42::handshake::product::kSignature2, static_cast<uint8_t>('2')};

        for (uint8_t byte : manufacturer) {
            push(byte);
        }
        for (uint8_t byte : family) {
            push(byte);
        }
        for (uint8_t byte : model) {
            push(byte);
        }

        std::array<uint8_t, 4> versionBytes{};
        const char *versionStr = FW_VERSION_STR;
        size_t cursor = 0;
        auto parseComponent = [&](size_t &pos) {
            uint16_t value = 0;
            bool foundDigit = false;
            while (versionStr[pos] != '\0') {
                char c = versionStr[pos];
                if (c >= '0' && c <= '9') {
                    foundDigit = true;
                    value = static_cast<uint16_t>(value * 10 + (c - '0'));
                    ++pos;
                } else {
                    if (foundDigit) {
                        break;
                    }
                    ++pos;
                }
            }
            if (value > 0x7F) {
                value = 0x7F;
            }
            return static_cast<uint8_t>(value);
        };

        versionBytes[0] = parseComponent(cursor);
        if (versionStr[cursor] == '.') {
            ++cursor;
        }
        versionBytes[1] = parseComponent(cursor);
        if (versionStr[cursor] == '.') {
            ++cursor;
        }
        versionBytes[2] = parseComponent(cursor);

        const char *gitSha = GIT_SHA_STR;
        uint8_t gitTag = 0;
        for (size_t i = 0; gitSha[i] != '\0'; ++i) {
            unsigned char c = static_cast<unsigned char>(gitSha[i]);
            if (c < 0x80) {
                gitTag = static_cast<uint8_t>(c);
                break;
            }
        }
        versionBytes[3] = gitTag;

        for (uint8_t byte : versionBytes) {
            push(byte & 0x7F);
        }

        push(0xF7);
        sendSysEx(reply.data(), static_cast<uint16_t>(idx));
    }
    _lastSysExLength = (length > sizeof(_lastSysEx)) ? sizeof(_lastSysEx) : length;
    for (uint16_t i = 0; i < _lastSysExLength; ++i) {
        _lastSysEx[i] = data[i];
    }
    _lastSysExType = SysExType::ManufacturerSpecific;
    _lastSysExSubId1 = _lastSysExSubId2 = 0;
    if (length >= 5) {
        uint8_t manufacturer = data[1];
        if (manufacturer == 0x7E || manufacturer == 0x7F) {
            _lastSysExType = (manufacturer == 0x7E) ? SysExType::UniversalNonRealTime
                                                    : SysExType::UniversalRealTime;
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
