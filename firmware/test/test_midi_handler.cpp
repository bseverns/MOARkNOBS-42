#if defined(UNIT_TEST) && defined(USB_MIDI_STUB)
#include "unity_config.h"
#define private public
#include "MIDIHandler.h"
#undef private
#include <unity.h>

void test_program_change() {
    MIDIHandler mh;
    mh.handleMIDI(midi::ProgramChange, 2, 45, 0);
    TEST_ASSERT_EQUAL_UINT8(45, MIDI.lastProgram);
    TEST_ASSERT_EQUAL_UINT8(2, MIDI.lastProgramChannel);
    TEST_ASSERT_EQUAL_UINT8(45, usbMIDI.lastProgram);
    TEST_ASSERT_EQUAL_UINT8(2, usbMIDI.lastProgramChannel);
}

void test_aftertouch() {
    MIDIHandler mh;
    mh.handleMIDI(midi::AfterTouchChannel, 3, 100, 0);
    TEST_ASSERT_EQUAL_UINT8(100, MIDI.lastAftertouch);
    TEST_ASSERT_EQUAL_UINT8(3, MIDI.lastAftertouchChannel);
    TEST_ASSERT_EQUAL_UINT8(100, usbMIDI.lastAftertouch);
    TEST_ASSERT_EQUAL_UINT8(3, usbMIDI.lastAftertouchChannel);
}

void test_mod_wheel() {
    MIDIHandler mh;
    MIDI.ccCount = usbMIDI.ccCount = 0;
    mh.sendModWheel(64, 2);
    TEST_ASSERT_EQUAL_UINT8(1, MIDI.ccCount);
    TEST_ASSERT_EQUAL_UINT8(1, MIDI.ccLog[0].control);
    TEST_ASSERT_EQUAL_UINT8(64, MIDI.ccLog[0].value);
    TEST_ASSERT_EQUAL_UINT8(2, MIDI.ccLog[0].channel);
    TEST_ASSERT_EQUAL_UINT8(1, usbMIDI.ccCount);
    TEST_ASSERT_EQUAL_UINT8(1, usbMIDI.ccLog[0].control);
    TEST_ASSERT_EQUAL_UINT8(64, usbMIDI.ccLog[0].value);
    TEST_ASSERT_EQUAL_UINT8(2, usbMIDI.ccLog[0].channel);
}

void test_pitch_bend() {
    MIDIHandler mh;
    uint8_t lsb = 0x00;
    uint8_t msb = 0x40; // center value 8192 -> bend 0
    mh.handleMIDI(midi::PitchBend, 1, lsb, msb);
    TEST_ASSERT_EQUAL_INT16(0, MIDI.lastPitchBend);
    TEST_ASSERT_EQUAL_UINT8(1, MIDI.lastPitchBendChannel);
    TEST_ASSERT_EQUAL_INT16(0, usbMIDI.lastPitchBend);
    TEST_ASSERT_EQUAL_UINT8(1, usbMIDI.lastPitchBendChannel);
}

void test_send_nrpn() {
    MIDIHandler mh;
    MIDI.ccCount = usbMIDI.ccCount = 0;
    mh.sendNRPN(0x1234, 0x5678, 3);
    TEST_ASSERT_EQUAL_UINT8(4, MIDI.ccCount);
    TEST_ASSERT_EQUAL_UINT8(99, MIDI.ccLog[0].control);
    TEST_ASSERT_EQUAL_UINT8(0x24, MIDI.ccLog[0].value);
    TEST_ASSERT_EQUAL_UINT8(98, MIDI.ccLog[1].control);
    TEST_ASSERT_EQUAL_UINT8(0x34, MIDI.ccLog[1].value);
    TEST_ASSERT_EQUAL_UINT8(6, MIDI.ccLog[2].control);
    TEST_ASSERT_EQUAL_UINT8(0xAC, MIDI.ccLog[2].value);
    TEST_ASSERT_EQUAL_UINT8(38, MIDI.ccLog[3].control);
    TEST_ASSERT_EQUAL_UINT8(0x78, MIDI.ccLog[3].value);
    TEST_ASSERT_EQUAL_UINT8(4, usbMIDI.ccCount);
}

void test_receive_nrpn() {
    MIDIHandler mh;
    uint16_t param = 0x1234;
    uint16_t value = 0x5678;
    uint8_t ch = 2;
    mh.handleMIDI(midi::ControlChange, ch, 99, (param >> 7) & 0x7F);
    mh.handleMIDI(midi::ControlChange, ch, 98, param & 0x7F);
    mh.handleMIDI(midi::ControlChange, ch, 6, (value >> 7) & 0x7F);
    mh.handleMIDI(midi::ControlChange, ch, 38, value & 0x7F);
    TEST_ASSERT_EQUAL_UINT16(param, mh.lastNRPNParam());
    TEST_ASSERT_EQUAL_UINT16(value, mh.lastNRPNValue());
}

void test_send_sysex() {
    MIDIHandler mh;
    uint8_t msg[] = {0xF0, 0x7D, 0x01, 0x02, 0xF7};
    MIDI.lastSysExLength = usbMIDI.lastSysExLength = 0;
    mh.sendSysEx(msg, sizeof(msg));
    TEST_ASSERT_EQUAL_UINT16(sizeof(msg), MIDI.lastSysExLength);
    TEST_ASSERT_EQUAL_UINT16(sizeof(msg), usbMIDI.lastSysExLength);
    for (uint8_t i = 0; i < sizeof(msg); ++i) {
        TEST_ASSERT_EQUAL_UINT8(msg[i], MIDI.lastSysEx[i]);
        TEST_ASSERT_EQUAL_UINT8(msg[i], usbMIDI.lastSysEx[i]);
    }
}

void test_drop_unsupported_usb_type() {
    MIDIHandler mh;
    MIDI.ccCount = usbMIDI.ccCount = 0;
    mh.clockTick = false;
    mh.lastExternalClock = mh.lastInternalTick = 0;
    usbMIDI.nextType = static_cast<midi::MidiType>(0x7F);
    usbMIDI.nextRead = true;
    mh.processIncomingMIDI();
    TEST_ASSERT_EQUAL_UINT8(0, MIDI.ccCount);
    TEST_ASSERT_EQUAL_UINT8(0, usbMIDI.ccCount);
    TEST_ASSERT_FALSE(mh.clockTick);
    TEST_ASSERT_EQUAL_UINT32(0, mh.lastExternalClock);
    TEST_ASSERT_EQUAL_UINT32(0, mh.lastInternalTick);
}

#endif // UNIT_TEST
