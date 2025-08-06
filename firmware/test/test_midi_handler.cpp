#include "USB-MIDI.h"
MidiInterfaceStub MIDI;
MidiInterfaceStub usbMIDI;
HardwareSerial Serial1;
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

void test_note_on_off() {
    MIDIHandler mh;
    mh.handleMIDI(midi::NoteOn, 4, 60, 100);
    TEST_ASSERT_EQUAL_UINT8(60, MIDI.lastNoteOn);
    TEST_ASSERT_EQUAL_UINT8(100, MIDI.lastNoteOnVelocity);
    TEST_ASSERT_EQUAL_UINT8(4, MIDI.lastNoteOnChannel);
    TEST_ASSERT_EQUAL_UINT8(60, usbMIDI.lastNoteOn);
    TEST_ASSERT_EQUAL_UINT8(100, usbMIDI.lastNoteOnVelocity);
    TEST_ASSERT_EQUAL_UINT8(4, usbMIDI.lastNoteOnChannel);
    mh.handleMIDI(midi::NoteOff, 4, 60, 0);
    TEST_ASSERT_EQUAL_UINT8(60, MIDI.lastNoteOff);
    TEST_ASSERT_EQUAL_UINT8(0, MIDI.lastNoteOffVelocity);
    TEST_ASSERT_EQUAL_UINT8(4, MIDI.lastNoteOffChannel);
    TEST_ASSERT_EQUAL_UINT8(60, usbMIDI.lastNoteOff);
    TEST_ASSERT_EQUAL_UINT8(0, usbMIDI.lastNoteOffVelocity);
    TEST_ASSERT_EQUAL_UINT8(4, usbMIDI.lastNoteOffChannel);
}

void test_send_control_change() {
    MIDIHandler mh;
    MIDI.ccCount = usbMIDI.ccCount = 0;
    mh.sendControlChange(20, 64, 3);
    TEST_ASSERT_EQUAL_UINT8(1, MIDI.ccCount);
    TEST_ASSERT_EQUAL_UINT8(20, MIDI.ccLog[0].control);
    TEST_ASSERT_EQUAL_UINT8(64, MIDI.ccLog[0].value);
    TEST_ASSERT_EQUAL_UINT8(3, MIDI.ccLog[0].channel);
    TEST_ASSERT_EQUAL_UINT8(1, usbMIDI.ccCount);
    TEST_ASSERT_EQUAL_UINT8(20, usbMIDI.ccLog[0].control);
    TEST_ASSERT_EQUAL_UINT8(64, usbMIDI.ccLog[0].value);
    TEST_ASSERT_EQUAL_UINT8(3, usbMIDI.ccLog[0].channel);
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
    TEST_ASSERT_EQUAL_UINT8(6,  MIDI.ccLog[2].control);
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
    mh.handleMIDI(midi::ControlChange, ch, 6,  (value >> 7) & 0x7F);
    mh.handleMIDI(midi::ControlChange, ch, 38, value & 0x7F);
    TEST_ASSERT_EQUAL_UINT16(param, mh.lastNRPNParam());
    TEST_ASSERT_EQUAL_UINT16(value, mh.lastNRPNValue());
}

void test_receive_nrpn_msb_only() {
    MIDIHandler mh;
    uint16_t param = 0x1234;
    uint8_t ch = 2;
    uint8_t valueMsb = 0x56;
    mh.handleMIDI(midi::ControlChange, ch, 99, (param >> 7) & 0x7F);
    mh.handleMIDI(midi::ControlChange, ch, 98, param & 0x7F);
    mh.handleMIDI(midi::ControlChange, ch, 6, valueMsb);
    TEST_ASSERT_EQUAL_UINT16(param, mh.lastNRPNParam());
    TEST_ASSERT_EQUAL_UINT16(valueMsb << 7, mh.lastNRPNValue());
}

void test_receive_nrpn_increment_decrement() {
    MIDIHandler mh;
    uint16_t param = 0x007F; // some param
    uint8_t ch = 1;
    mh.handleMIDI(midi::ControlChange, ch, 99, (param >> 7) & 0x7F);
    mh.handleMIDI(midi::ControlChange, ch, 98, param & 0x7F);
    mh.handleMIDI(midi::ControlChange, ch, 6, 0x01);
    TEST_ASSERT_EQUAL_UINT16(param, mh.lastNRPNParam());
    TEST_ASSERT_EQUAL_UINT16(0x80, mh.lastNRPNValue());
    mh.handleMIDI(midi::ControlChange, ch, 96, 0);
    TEST_ASSERT_EQUAL_UINT16(0x81, mh.lastNRPNValue());
    mh.handleMIDI(midi::ControlChange, ch, 97, 0);
    TEST_ASSERT_EQUAL_UINT16(0x80, mh.lastNRPNValue());
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

void setUp() {}
void tearDown() {}

void setup() {
    UNITY_BEGIN();
    RUN_TEST(test_program_change);
    RUN_TEST(test_aftertouch);
    RUN_TEST(test_pitch_bend);
    RUN_TEST(test_note_on_off);
    RUN_TEST(test_send_control_change);
    RUN_TEST(test_send_nrpn);
    RUN_TEST(test_receive_nrpn);
    RUN_TEST(test_receive_nrpn_msb_only);
    RUN_TEST(test_receive_nrpn_increment_decrement);
    RUN_TEST(test_send_sysex);
    UNITY_END();
}

void loop() {}
