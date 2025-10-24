#if defined(UNIT_TEST) && defined(USB_MIDI_STUB)
#include "unity_config.h"
#define private public
#include "MIDIHandler.h"
#undef private
#include <unity.h>
#include "interop/mn42_map.h"
#include "version.h"
#include "Globals.h"
#include "ConfigManager.h"
#include "TestHelpers.h"
#include "Utility.h"
#include <EEPROM.h>

#include <algorithm>
#include <array>
#include <vector>

namespace {
struct UsbMidiGuard {
    UsbMidiGuard() : previous(g_usbMidiOutEnabled) { g_usbMidiOutEnabled = true; }
    ~UsbMidiGuard() { g_usbMidiOutEnabled = previous; }
    bool previous;
};

struct StubEnvelope {
    // Unity wants a baseline that sounds like the shipping rig.
    // The pedals leave sustain parked at 96, so we bake that here and let
    // outlier specs override it when they feel like getting weird.
    static constexpr uint8_t kDefaultSustain = 96;

    explicit StubEnvelope(uint8_t sustain = kDefaultSustain) : level(sustain) {}
    uint8_t getEnvelopeLevel() const { return level; }

  private:
    uint8_t level;
};

void resetMidiTransports() {
    MIDI.ccCount = 0;
    MIDI.ccTotal = 0;
    MIDI.ccOverflow = false;
    MIDI.lastSysExLength = 0;
    MIDI.sysExTotal = 0;
    MIDI.sysExOverflow = false;
    MIDI.lastNoteOn = 0;
    MIDI.lastNoteOnVelocity = 0;
    MIDI.lastNoteOnChannel = 0;
    MIDI.lastNoteOff = 0;
    MIDI.lastNoteOffVelocity = 0;
    MIDI.lastNoteOffChannel = 0;
    MIDI.lastProgram = 0;
    MIDI.lastProgramChannel = 0;
    MIDI.lastAftertouch = 0;
    MIDI.lastAftertouchChannel = 0;
    MIDI.lastPitchBend = 0;
    MIDI.lastPitchBendChannel = 0;
    std::fill_n(MIDI.lastSysEx, kSysExCapacity, 0);

    usbMIDI.ccCount = 0;
    usbMIDI.ccTotal = 0;
    usbMIDI.ccOverflow = false;
    usbMIDI.lastSysExLength = 0;
    usbMIDI.sysExTotal = 0;
    usbMIDI.sysExOverflow = false;
    usbMIDI.lastNoteOn = 0;
    usbMIDI.lastNoteOnVelocity = 0;
    usbMIDI.lastNoteOnChannel = 0;
    usbMIDI.lastNoteOff = 0;
    usbMIDI.lastNoteOffVelocity = 0;
    usbMIDI.lastNoteOffChannel = 0;
    usbMIDI.lastProgram = 0;
    usbMIDI.lastProgramChannel = 0;
    usbMIDI.lastAftertouch = 0;
    usbMIDI.lastAftertouchChannel = 0;
    usbMIDI.lastPitchBend = 0;
    usbMIDI.lastPitchBendChannel = 0;
    std::fill_n(usbMIDI.lastSysEx, kSysExCapacity, 0);
}
} // namespace

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

// When the firmware blasts a flood of CC writes we expect every legal event to
// make it across both transports without tripping the overflow flags.
void test_pot_burst_keeps_cc_counters_honest() {
    UsbMidiGuard guard;
    resetMidiTransports();

    MIDIHandler mh;
    mh._txCount = 0;

    ConfigManager cfg(NUM_POTS, NUM_BUTTONS);
    MIDISlot &slot = cfg.getSlot(0);
    slot.active = true;
    slot.type = MIDIMessageType::CC;
    slot.midiChannel = 3;
    slot.data1 = 74;

    constexpr uint16_t kBursts = 96;
    for (uint16_t i = 0; i < kBursts; ++i) {
        uint16_t raw = static_cast<uint16_t>((i * 37) % 1024);
        uint8_t mapped = Utility::mapToMidiValue(raw);
        mh.sendControlChange(slot.data1, mapped, slot.midiChannel);
    }

    TEST_ASSERT_EQUAL_UINT32(kBursts, mh._txCount);
    TEST_ASSERT_EQUAL_UINT32(kBursts, MIDI.ccTotal);
    TEST_ASSERT_EQUAL_UINT32(kBursts, usbMIDI.ccTotal);
    TEST_ASSERT_FALSE(MIDI.ccOverflow);
    TEST_ASSERT_FALSE(usbMIDI.ccOverflow);
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
    UsbMidiGuard guard;
    resetMidiTransports();

    MIDIHandler mh;
    uint8_t msg[] = {0xF0, 0x7D, 0x01, 0x02, 0xF7};
    mh.sendSysEx(msg, sizeof(msg));
    TEST_ASSERT_EQUAL_UINT16(sizeof(msg), MIDI.lastSysExLength);
    TEST_ASSERT_EQUAL_UINT16(sizeof(msg), usbMIDI.lastSysExLength);
    TEST_ASSERT_EQUAL_UINT16(sizeof(msg), MIDI.sysExTotal);
    TEST_ASSERT_EQUAL_UINT16(sizeof(msg), usbMIDI.sysExTotal);
    TEST_ASSERT_FALSE(MIDI.sysExOverflow);
    TEST_ASSERT_FALSE(usbMIDI.sysExOverflow);
    for (uint8_t i = 0; i < sizeof(msg); ++i) {
        TEST_ASSERT_EQUAL_UINT8(msg[i], MIDI.lastSysEx[i]);
        TEST_ASSERT_EQUAL_UINT8(msg[i], usbMIDI.lastSysEx[i]);
    }
}

// Guardrail for the transport logging: long payloads should survive the trip
// without getting truncated unless we exceed the stub's capacity.
void test_long_sysex_payload_round_trips() {
    UsbMidiGuard guard;
    resetMidiTransports();

    MIDIHandler mh;

    std::array<uint8_t, 120> payload{};
    payload[0] = 0xF0;
    for (size_t i = 1; i < payload.size() - 1; ++i) {
        payload[i] = static_cast<uint8_t>((i * 7) & 0x7F);
    }
    payload.back() = 0xF7;

    mh.sendSysEx(payload.data(), static_cast<uint16_t>(payload.size()));

    TEST_ASSERT_EQUAL_UINT16(payload.size(), MIDI.lastSysExLength);
    TEST_ASSERT_EQUAL_UINT16(payload.size(), usbMIDI.lastSysExLength);
    TEST_ASSERT_EQUAL_UINT16(payload.size(), MIDI.sysExTotal);
    TEST_ASSERT_EQUAL_UINT16(payload.size(), usbMIDI.sysExTotal);
    TEST_ASSERT_FALSE(MIDI.sysExOverflow);
    TEST_ASSERT_FALSE(usbMIDI.sysExOverflow);
    for (size_t i = 0; i < payload.size(); ++i) {
        TEST_ASSERT_EQUAL_UINT8(payload[i], MIDI.lastSysEx[i]);
        TEST_ASSERT_EQUAL_UINT8(payload[i], usbMIDI.lastSysEx[i]);
    }
}

// While MIDI is streaming, flip a slot's mode and make sure we keep emitting
// legit traffic without blowing out the counters.
void test_config_mutation_during_stream_stays_valid() {
    UsbMidiGuard guard;
    resetMidiTransports();

    MIDIHandler mh;
    mh._txCount = 0;

    ConfigManager cfg(NUM_POTS, NUM_BUTTONS);
    MIDISlot &slot = cfg.getSlot(0);
    slot.active = true;
    slot.type = MIDIMessageType::CC;
    slot.midiChannel = 4;
    slot.data1 = 42;
    slot.ef.followerIndex = 0;

    StubEnvelope env{110};
    uint32_t ccEvents = 0;
    uint32_t noteEvents = 0;

    constexpr uint16_t kUpdates = 40;
    for (uint16_t i = 0; i < kUpdates; ++i) {
        uint16_t raw = static_cast<uint16_t>((i * 23) % 1024);
        uint8_t mapped = Utility::mapToMidiValue(raw);

        if (i == kUpdates / 2) {
            slot.type = MIDIMessageType::Note;
        }

        if (slot.type == MIDIMessageType::CC) {
            mh.sendControlChange(slot.data1, mapped, slot.midiChannel);
            ++ccEvents;
        } else {
            uint8_t note = static_cast<uint8_t>(Utility::mapToMidiValue(raw) % 128);
            uint8_t velocity = env.getEnvelopeLevel();
            mh.sendNoteOn(note, velocity, slot.midiChannel);
            mh.sendNoteOff(note, 0, slot.midiChannel);
            ++noteEvents;
        }
    }

    TEST_ASSERT_EQUAL_UINT32(kUpdates / 2, ccEvents);
    TEST_ASSERT_EQUAL_UINT32(kUpdates / 2, noteEvents);
    TEST_ASSERT_EQUAL_UINT32(ccEvents + noteEvents * 2, mh._txCount);
    TEST_ASSERT_EQUAL_UINT8(slot.midiChannel, usbMIDI.lastNoteOnChannel);
    TEST_ASSERT_EQUAL_UINT8(slot.midiChannel, MIDI.lastNoteOnChannel);
    TEST_ASSERT_FALSE(MIDI.ccOverflow);
    TEST_ASSERT_FALSE(usbMIDI.ccOverflow);
}

// Universal Identity Requests should get a full reply that mirrors across both
// transports and advertises our firmware fingerprints.
void test_sysex_identity_request_reply() {
    MIDIHandler mh;
    UsbMidiGuard guard;
    resetMidiTransports();

    MIDI.lastSysExLength = usbMIDI.lastSysExLength = 0;
    const uint8_t request[] = {0xF0, 0x7E, 0x42, 0x06, 0x01, 0xF7};
    mh.handleSysEx(request, sizeof(request));

    constexpr size_t kIdentityReplyLength = 15;
    std::array<uint8_t, kIdentityReplyLength> expected{};
    size_t idx = 0;
    auto push = [&](uint8_t value) {
        if (idx < expected.size()) {
            expected[idx++] = value;
        }
    };

    push(0xF0);
    push(0x7E);
    push(request[2]);
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

    TEST_ASSERT_EQUAL_UINT16(idx, MIDI.lastSysExLength);
    TEST_ASSERT_EQUAL_UINT16(idx, usbMIDI.lastSysExLength);
    for (size_t i = 0; i < idx; ++i) {
        TEST_ASSERT_EQUAL_UINT8(expected[i], MIDI.lastSysEx[i]);
        TEST_ASSERT_EQUAL_UINT8(expected[i], usbMIDI.lastSysEx[i]);
    }
}

// Garbage in? Garbage out. Malformed SysEx payloads should be ignored so we
// don't clobber the state sniffed by diagnostics.
void test_handle_sysex_rejects_bad_framing() {
    MIDIHandler mh;
    mh._lastSysExLength = 4;
    mh._lastSysExType = SysExType::UniversalRealTime;
    mh._rxCount = 3;

    uint8_t bogus[] = {0x7D, 0x01, 0x02};
    mh.handleSysEx(bogus, sizeof(bogus));

    TEST_ASSERT_EQUAL_UINT16(4, mh._lastSysExLength);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SysExType::UniversalRealTime),
                            static_cast<uint8_t>(mh._lastSysExType));
    TEST_ASSERT_EQUAL_UINT32(3, mh._rxCount);
}

void test_handle_sysex_drops_oversize() {
    MIDIHandler mh;
    mh._lastSysExLength = 5;
    mh._rxCount = 2;

    std::array<uint8_t, 70> payload{};
    payload[0] = 0xF0;
    payload[payload.size() - 1] = 0xF7;
    mh.handleSysEx(payload.data(), static_cast<uint16_t>(payload.size()));

    TEST_ASSERT_EQUAL_UINT16(5, mh._lastSysExLength);
    TEST_ASSERT_EQUAL_UINT32(2, mh._rxCount);
}

// When the serial queue is bursting at the seams with repeated CCs, the newest
// value should be the one that survives and older duplicates must get axed.
void test_serial_queue_coalesces_latest_value() {
    MIDIHandler mh;

    auto base = mh.makeControlChange(3, 74, 0);
    auto fresh = mh.makeControlChange(3, 74, 0x7F);
    auto program = mh.makeProgramChange(5, 17);
    auto note = mh.makeNoteOn(6, 64, 96);

    for (size_t i = 0; i < MIDIHandler::kSerialQueueSize; ++i) {
        mh._serialQueue[i] = base;
        mh._serialQueue[i].data2 = static_cast<uint8_t>(i & 0x7F);
    }
    mh._serialQueue[0] = program;
    mh._serialQueue[3] = note;
    mh._serialQueueHead = 0;
    mh._serialQueueTail = 0;
    mh._serialQueueFull = true;

    TEST_ASSERT_TRUE(mh.enqueueSerialMessage(fresh));

    TEST_ASSERT_FALSE(mh._serialQueueFull);
    TEST_ASSERT_EQUAL_UINT32(3, mh.serialQueueSize());
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(MIDIHandler::SerialMessageType::ProgramChange),
                            static_cast<uint8_t>(mh._serialQueue[0].type));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(MIDIHandler::SerialMessageType::NoteOn),
                            static_cast<uint8_t>(mh._serialQueue[1].type));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(MIDIHandler::SerialMessageType::ControlChange),
                            static_cast<uint8_t>(mh._serialQueue[2].type));
    TEST_ASSERT_EQUAL_UINT8(0x7F, mh._serialQueue[2].data2);
}

// DIN pacing can hold a message back for a few hundred microseconds; make sure
// the idle polling path keeps draining the queue even without new traffic.
void test_process_pumps_serial_queue() {
    MIDIHandler mh;
    MIDI.ccCount = 0;

    auto msg = mh.makeControlChange(1, 99, 23);
    mh.enqueueSerialMessage(msg);
    mh._lastSerialSendUs = micros();

    mh.serviceSerialQueue();
    TEST_ASSERT_EQUAL_UINT8(0, MIDI.ccCount);
    TEST_ASSERT_EQUAL_UINT32(1, mh.serialQueueSize());

    mh._lastSerialSendUs =
        micros() - static_cast<uint32_t>(msg.byteCount) * MIDIHandler::kSerialByteMicros;

    mh.processIncomingMIDI();

    TEST_ASSERT_EQUAL_UINT32(0, mh.serialQueueSize());
    TEST_ASSERT_EQUAL_UINT8(1, MIDI.ccCount);
    TEST_ASSERT_EQUAL_UINT8(99, MIDI.ccLog[0].control);
    TEST_ASSERT_EQUAL_UINT8(23, MIDI.ccLog[0].value);
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

// Schema 0x0004 should vaporise legacy 6-byte slot data before we start reading.
void test_config_manager_wipes_legacy_slot_stride() {
    // Pretend we just flashed over a 0x0002 build.
    constexpr uint16_t kLegacyVersion = 0x0002;
    EEPROM.put(EEPROM_CONFIG_VERSION, kLegacyVersion);

    // Backfill the old 6-byte stride so the wipe has something obvious to nuke.
    for (uint8_t slot = 0; slot < NUM_SLOTS; ++slot) {
        const uint16_t legacyAddress = static_cast<uint16_t>(EEPROM_SLOT_BASE + slot * 6);
        for (uint8_t byte = 0; byte < 6; ++byte) {
            EEPROM.update(static_cast<int>(legacyAddress + byte), 0x7E);
        }
    }

    // Drop breadcrumbs into the profile blocks; the sanitizer should zero them.
    EEPROM.update(static_cast<int>(EEPROM_PROFILE_START(1)), 0xA5);
    EEPROM.update(static_cast<int>(EEPROM_PROFILE_START(2)), 0x5A);

    ConfigManager cfg = createConfigManager();
    std::vector<uint8_t> pots;
    cfg.begin(pots);

    MIDISlot stored{};
    EEPROM.get(static_cast<int>(EEPROM_SLOT_BASE), stored);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(MIDIMessageType::OFF),
                            static_cast<uint8_t>(stored.type));
    TEST_ASSERT_EQUAL_UINT8(1, stored.midiChannel);
    TEST_ASSERT_EQUAL_UINT8(0, stored.sysexLength);
    for (uint8_t i = 0; i < SysExTemplate::kMaxLength; ++i) {
        TEST_ASSERT_EQUAL_UINT8(0, stored.sysexTemplate[i]);
    }

    TEST_ASSERT_EQUAL_UINT8(0x00, EEPROM.read(static_cast<int>(EEPROM_PROFILE_START(1))));
    TEST_ASSERT_EQUAL_UINT8(0x00, EEPROM.read(static_cast<int>(EEPROM_PROFILE_START(2))));
}

#endif // UNIT_TEST
