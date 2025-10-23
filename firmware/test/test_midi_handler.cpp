#if defined(UNIT_TEST) && defined(USB_MIDI_STUB)
#include "unity_config.h"
#define private public
#include "MIDIHandler.h"
#undef private
#include <unity.h>
#include "interop/mn42_map.h"
#include "version.h"

#include <array>
#include <vector>

extern bool g_clockOutEnabled;

// MIDIHandler has a ridiculous number of responsibilities—USB mirror writes,
// serial fan-out, NRPN parsing, SysEx scratch buffers, and the internal clock
// tick bookkeeping that keeps the arpeggiator honest.  These Unity specs lean
// on the stub transport to make sure every leg of that routing table still
// behaves with the same swagger the hardware build expects.

// Smoke the Program Change path and ensure both DIN and USB mirrors agree.
void test_program_change() {
    MIDIHandler mh;
    mh.handleMIDI(midi::ProgramChange, 2, 45, 0);
    TEST_ASSERT_EQUAL_UINT8(45, MIDI.lastProgram);
    TEST_ASSERT_EQUAL_UINT8(2, MIDI.lastProgramChannel);
    TEST_ASSERT_EQUAL_UINT8(45, usbMIDI.lastProgram);
    TEST_ASSERT_EQUAL_UINT8(2, usbMIDI.lastProgramChannel);
}

// Aftertouch is edge-case city: double-check it hits both transports verbatim.
void test_aftertouch() {
    MIDIHandler mh;
    mh.handleMIDI(midi::AfterTouchChannel, 3, 100, 0);
    TEST_ASSERT_EQUAL_UINT8(100, MIDI.lastAftertouch);
    TEST_ASSERT_EQUAL_UINT8(3, MIDI.lastAftertouchChannel);
    TEST_ASSERT_EQUAL_UINT8(100, usbMIDI.lastAftertouch);
    TEST_ASSERT_EQUAL_UINT8(3, usbMIDI.lastAftertouchChannel);
}

// The mod wheel is one of the CC hot paths; verify the fan-out log stays in
// sync and that we don't silently drop channels or data bytes.
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

// Pitch bend flows through the 14-bit code path.  This run keeps the math sane
// so middle detents land back at zero instead of wobbling.
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

// NRPN writes are a four-message handshake.  This test ensures we emit the
// proper CC sequence and that the stub recorder remembers the lot.
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

// Same NRPN dance but from the receiver side—assemble a packet manually and
// confirm the decoded param/value pair we stash for diagnostics.
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

// SysEx is our bulk config escape hatch.  Make sure the handler copies the
// payload byte-for-byte so higher layers can rehydrate it later.
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

// Universal Identity Requests should get a full reply that mirrors across both
// transports and advertises our firmware fingerprints.
void test_sysex_identity_request_reply() {
    MIDIHandler mh;
    struct UsbMidiGuard {
        UsbMidiGuard() : previous(g_usbMidiOutEnabled) { g_usbMidiOutEnabled = true; }
        ~UsbMidiGuard() { g_usbMidiOutEnabled = previous; }
        bool previous;
    } guard;

    MIDI.lastSysExLength = usbMIDI.lastSysExLength = 0;
    const uint8_t request[] = {0xF0, 0x7E, 0x42, 0x06, 0x01, 0xF7};
    mh.handleSysEx(request, sizeof(request));

    std::vector<uint8_t> expected;
    expected.reserve(32);
    expected.push_back(0xF0);
    expected.push_back(0x7E);
    expected.push_back(request[2]);
    expected.push_back(0x06);
    expected.push_back(0x02);
    expected.push_back(seedbox::interop::mn42::handshake::product::kManufacturerId);
    expected.push_back(seedbox::interop::mn42::handshake::product::kSignature0);
    expected.push_back(seedbox::interop::mn42::handshake::product::kSignature1);
    expected.push_back(seedbox::interop::mn42::handshake::product::kSignature2);
    expected.push_back(seedbox::interop::mn42::handshake::product::kSignature0);
    expected.push_back(seedbox::interop::mn42::handshake::product::kSignature1);
    expected.push_back(seedbox::interop::mn42::handshake::product::kSignature2);
    expected.push_back(seedbox::interop::mn42::handshake::product::kPresenceFlag);

    std::array<uint8_t, 4> versionDigits{};
    const char *versionStr = FW_VERSION_STR;
    size_t versionCount = 0;
    for (size_t i = 0; versionStr[i] != '\0' && versionCount < versionDigits.size(); ++i) {
        char c = versionStr[i];
        if (c >= '0' && c <= '9') {
            versionDigits[versionCount++] = static_cast<uint8_t>(c - '0');
        }
    }
    while (versionCount < versionDigits.size()) {
        versionDigits[versionCount++] = 0;
    }
    expected.insert(expected.end(), versionDigits.begin(), versionDigits.end());

    expected.push_back('g');
    expected.push_back('i');
    expected.push_back('t');
    expected.push_back('-');
    const char *gitSha = GIT_SHA_STR;
    for (size_t i = 0; gitSha[i] != '\0' && i < 4; ++i) {
        expected.push_back(static_cast<uint8_t>(gitSha[i]));
    }
    expected.push_back(0xF7);

    TEST_ASSERT_EQUAL_UINT16(expected.size(), MIDI.lastSysExLength);
    TEST_ASSERT_EQUAL_UINT16(expected.size(), usbMIDI.lastSysExLength);
    for (size_t i = 0; i < expected.size(); ++i) {
        TEST_ASSERT_EQUAL_UINT8(expected[i], MIDI.lastSysEx[i]);
        TEST_ASSERT_EQUAL_UINT8(expected[i], usbMIDI.lastSysEx[i]);
    }
}

// If usbMIDI throws a curveball message type the firmware doesn't support we
// should shrug and move on, not crash or mutate state.
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

// Taps into usbMIDI's fake clock message to ensure we bump the edge counter
// and leave a bread crumb for whoever polls ::isClockTick().
void test_usb_clock_tick_advances_counter() {
    MIDIHandler mh;
    mh.clockTick = false;
    mh._clockTickCount = 0;
    usbMIDI.nextRead = true;
    usbMIDI.nextType = midi::Tick;

    mh.processIncomingMIDI();

    TEST_ASSERT_EQUAL_UINT32(1, mh.clockTickCount());
    TEST_ASSERT_TRUE(mh.isClockTick());
    mh.clearClockTick();
    TEST_ASSERT_FALSE(mh.isClockTick());
}

// Exercise the internal clock generator and make sure the transmit counter
// ticks along with the virtual pulse when the global clock out flag is hot.
void test_generate_clock_tick_advances_counter() {
    MIDIHandler mh;
    mh.clockTick = false;
    mh._clockTickCount = 0;
    mh._txCount = 0;
    g_clockOutEnabled = true;

    mh.generateClockTick();

    TEST_ASSERT_EQUAL_UINT32(1, mh.clockTickCount());
    TEST_ASSERT_TRUE(mh.isClockTick());
    TEST_ASSERT_EQUAL_UINT32(1, mh._txCount);

    mh.clearClockTick();
    g_clockOutEnabled = false;
}

#endif // UNIT_TEST
