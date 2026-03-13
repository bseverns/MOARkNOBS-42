#if defined(UNIT_TEST) && defined(USB_MIDI_STUB)
#include "unity_config.h"
#include <unity.h>

#include "Globals.h"
#include "MIDIHandler.h"
#include "TimeStub.h"
#include "interop/SeedBoxLink.h"
#include "interop/mn42_map.h"

#include <algorithm>

namespace {

using seedbox::interop::mn42::SeedBoxLink;
using namespace seedbox::interop::mn42;

struct UsbMidiGuard {
    UsbMidiGuard() : previous(g_usbMidiOutEnabled) { g_usbMidiOutEnabled = true; }
    ~UsbMidiGuard() { g_usbMidiOutEnabled = previous; }
    bool previous;
};

void resetMidiTransports() {
    MIDI.ccCount = 0;
    MIDI.ccTotal = 0;
    MIDI.ccOverflow = false;
    MIDI.lastSysExLength = 0;
    MIDI.sysExTotal = 0;
    MIDI.sysExOverflow = false;
    std::fill_n(MIDI.lastSysEx, kSysExCapacity, 0);

    usbMIDI.ccCount = 0;
    usbMIDI.ccTotal = 0;
    usbMIDI.ccOverflow = false;
    usbMIDI.lastSysExLength = 0;
    usbMIDI.sysExTotal = 0;
    usbMIDI.sysExOverflow = false;
    std::fill_n(usbMIDI.lastSysEx, kSysExCapacity, 0);
}

const MidiInterfaceStub::CCEvent &lastCcEvent() { return MIDI.ccLog[MIDI.ccCount - 1]; }

void assertLastCc(uint8_t control, uint8_t value, uint8_t channel) {
    TEST_ASSERT_GREATER_THAN_UINT8(0, MIDI.ccCount);
    const auto &event = lastCcEvent();
    TEST_ASSERT_EQUAL_UINT8(control, event.control);
    TEST_ASSERT_EQUAL_UINT8(value, event.value);
    TEST_ASSERT_EQUAL_UINT8(channel, event.channel);
}

} // namespace

void test_seedbox_link_begin_sends_hello_and_identity_ping() {
    UsbMidiGuard guard;
    resetMidiTransports();
    g_fakeNowMs = 0;

    MIDIHandler handler;
    SeedBoxLink &link = SeedBoxLink::instance();
    link.begin(&handler);

    TEST_ASSERT_FALSE(link.hasAck());
    TEST_ASSERT_EQUAL_UINT8(1, MIDI.ccCount);
    assertLastCc(cc::kHandshake, handshake::kHello, kDefaultChannel);

    static constexpr uint8_t kExpectedIdentity[] = {0xF0,
                                                    handshake::product::kManufacturerId,
                                                    handshake::product::kSignature0,
                                                    handshake::product::kSignature1,
                                                    handshake::product::kSignature2,
                                                    handshake::product::kPresenceFlag,
                                                    0xF7};
    TEST_ASSERT_EQUAL_UINT16(sizeof(kExpectedIdentity), MIDI.lastSysExLength);
    TEST_ASSERT_EQUAL_UINT16(sizeof(kExpectedIdentity), MIDI.sysExTotal);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(kExpectedIdentity, MIDI.lastSysEx, sizeof(kExpectedIdentity));
}

void test_seedbox_link_hello_triggers_ack() {
    UsbMidiGuard guard;
    resetMidiTransports();
    g_fakeNowMs = 0;

    MIDIHandler handler;
    SeedBoxLink &link = SeedBoxLink::instance();
    link.begin(&handler);
    resetMidiTransports();

    TEST_ASSERT_TRUE(link.handleControlChange(kDefaultChannel, cc::kHandshake, handshake::kHello));
    TEST_ASSERT_FALSE(link.hasAck());
    TEST_ASSERT_TRUE(link.peerAlive());
    TEST_ASSERT_EQUAL_UINT8(1, MIDI.ccCount);
    assertLastCc(cc::kHandshake, handshake::kAck, kDefaultChannel);
}

void test_seedbox_link_keepalive_and_timeout_restart_handshake() {
    UsbMidiGuard guard;
    resetMidiTransports();
    g_fakeNowMs = 0;

    MIDIHandler handler;
    SeedBoxLink &link = SeedBoxLink::instance();
    link.begin(&handler);

    TEST_ASSERT_TRUE(link.handleControlChange(kDefaultChannel, cc::kHandshake, handshake::kAck));
    TEST_ASSERT_TRUE(link.hasAck());
    TEST_ASSERT_TRUE(link.peerAlive());

    resetMidiTransports();
    advanceMs(3000);
    link.update();
    TEST_ASSERT_TRUE(link.hasAck());
    TEST_ASSERT_EQUAL_UINT8(1, MIDI.ccCount);
    assertLastCc(cc::kHandshake, handshake::kKeepAlive, kDefaultChannel);

    resetMidiTransports();
    advanceMs(5001);
    link.update();
    TEST_ASSERT_FALSE(link.hasAck());
    TEST_ASSERT_FALSE(link.peerAlive());
    TEST_ASSERT_EQUAL_UINT8(2, MIDI.ccCount);
    TEST_ASSERT_EQUAL_UINT8(handshake::kKeepAlive, MIDI.ccLog[0].value);
    TEST_ASSERT_EQUAL_UINT8(handshake::kHello, MIDI.ccLog[1].value);
    TEST_ASSERT_EQUAL_UINT8(cc::kHandshake, MIDI.ccLog[0].control);
    TEST_ASSERT_EQUAL_UINT8(cc::kHandshake, MIDI.ccLog[1].control);
    TEST_ASSERT_EQUAL_UINT8(kDefaultChannel, MIDI.ccLog[0].channel);
    TEST_ASSERT_EQUAL_UINT8(kDefaultChannel, MIDI.ccLog[1].channel);
}
#endif
