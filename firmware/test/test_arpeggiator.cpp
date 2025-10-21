#include "unity_config.h" // yanks in Arduino and our usbMIDI doppelganger
#include <unity.h>

#define private public
#include "MIDIHandler.h"
#include "PotentiometerManager.h"
#undef private

#include "Arpeggiator.h"
#include "ConfigManager.h"
#include "Globals.h"
#include "Utility.h"
#include "usb_midi.h"

namespace {
struct MidiUsbGuard {
    MidiUsbGuard() {
        g_usbMidiOutEnabled = true;
        reset();
    }
    ~MidiUsbGuard() { g_usbMidiOutEnabled = false; }
    void reset() {
        usbMIDI.lastNoteOn = 0;
        usbMIDI.lastNoteOnVelocity = 0;
        usbMIDI.lastNoteOnChannel = 0;
        usbMIDI.lastNoteOff = 0;
        usbMIDI.lastNoteOffVelocity = 0;
        usbMIDI.lastNoteOffChannel = 0;
    }
};

PotentiometerManager makePots() {
    PotentiometerManager pots(primaryMuxPins, secondaryMuxPins, potMuxAnalogPin);
    for (uint8_t i = 0; i < NUM_POTS; ++i) {
        pots.potLastValues[i] = 0;
    }
    return pots;
}

ConfigManager makeConfig() {
    ConfigManager cfg(NUM_POTS, NUM_BUTTONS);
    for (uint8_t i = 0; i < NUM_SLOTS; ++i) {
        MIDISlot &slot = cfg.getSlot(i);
        slot.active = false;
        slot.type = MIDIMessageType::OFF;
        slot.midiChannel = 1;
        slot.data1 = 0;
        slot.arpNote = 0;
    }
    return cfg;
}

MIDIHandler primeMidi() {
    MIDIHandler midi;
    midi.clockTick = false;
    midi._clockTickCount = 0;
    midi._txCount = 0;
    return midi;
}

void prepSlot(ConfigManager &cfg, uint8_t idx, MIDIMessageType type, uint8_t channel,
              uint8_t arpNote) {
    MIDISlot &slot = cfg.getSlot(idx);
    slot.active = true;
    slot.type = type;
    slot.midiChannel = channel;
    slot.arpNote = arpNote;
}
} // namespace

// Unity expects these, even if they just wave from the sidelines.
void setUp() {}
void tearDown() {}

void test_start_stop_cycle() {
    Arpeggiator arp;
    TEST_ASSERT_FALSE(arp.isActive());
    arp.start(7);
    TEST_ASSERT_TRUE(arp.isActive());
    TEST_ASSERT_EQUAL_UINT8(7, arp.getSlot());
    arp.stop();
    TEST_ASSERT_FALSE(arp.isActive());
}

void test_pot_root_drives_default() {
    MidiUsbGuard guard;
    Arpeggiator arp;
    arp.setLength(1);
    arp.setPatternLength(1);
    arp.setBaseNoteSource(Arpeggiator::BaseNoteSource::Pot);
    arp.start(0);

    auto cfg = makeConfig();
    prepSlot(cfg, 0, MIDIMessageType::Note, 2, 10);

    auto pots = makePots();
    pots.potLastValues[0] = 1023; // slam the knob to max

    MIDIHandler midi = primeMidi();
    arp.update(midi, cfg, pots); // latch clock baseline
    midi._clockTickCount++;
    arp.update(midi, cfg, pots);

    TEST_ASSERT_EQUAL_UINT8(127, usbMIDI.lastNoteOn);
    TEST_ASSERT_EQUAL_UINT8(127, usbMIDI.lastNoteOnVelocity);
    TEST_ASSERT_EQUAL_UINT8(2, usbMIDI.lastNoteOnChannel);
    TEST_ASSERT_EQUAL_UINT8(127, cfg.getSlot(0).arpNote);
}

void test_slot_root_wins_over_pot() {
    MidiUsbGuard guard;
    Arpeggiator arp;
    arp.setLength(1);
    arp.setPatternLength(1);
    arp.setBaseNoteSource(Arpeggiator::BaseNoteSource::Slot);
    arp.start(0);

    auto cfg = makeConfig();
    prepSlot(cfg, 0, MIDIMessageType::Note, 1, 60);

    auto pots = makePots();
    pots.potLastValues[0] = 1023; // would map to 127 if we trusted the pot

    MIDIHandler midi = primeMidi();
    arp.update(midi, cfg, pots);
    midi._clockTickCount++;
    arp.update(midi, cfg, pots);

    TEST_ASSERT_EQUAL_UINT8(60, usbMIDI.lastNoteOn);
    TEST_ASSERT_EQUAL_UINT8(60, usbMIDI.lastNoteOnVelocity);
    TEST_ASSERT_EQUAL_UINT8(1, usbMIDI.lastNoteOnChannel);
    TEST_ASSERT_EQUAL_UINT8(60, cfg.getSlot(0).arpNote);
}

void test_external_callback_sets_root() {
    MidiUsbGuard guard;
    Arpeggiator arp;
    arp.setLength(1);
    arp.setPatternLength(1);
    arp.setBaseNoteSource(Arpeggiator::BaseNoteSource::External);
    arp.setBaseNoteCallback([]() { return static_cast<uint8_t>(72); });
    arp.start(1);

    auto cfg = makeConfig();
    prepSlot(cfg, 1, MIDIMessageType::Note, 3, 48);

    auto pots = makePots();
    pots.potLastValues[1] = 256; // stray value that should be ignored

    MIDIHandler midi = primeMidi();
    arp.update(midi, cfg, pots);
    midi._clockTickCount++;
    arp.update(midi, cfg, pots);

    TEST_ASSERT_EQUAL_UINT8(72, usbMIDI.lastNoteOn);
    TEST_ASSERT_EQUAL_UINT8(72, cfg.getSlot(1).arpNote);
    TEST_ASSERT_EQUAL_UINT8(3, usbMIDI.lastNoteOnChannel);
}

void test_external_base_note_without_callback() {
    MidiUsbGuard guard;
    Arpeggiator arp;
    arp.setLength(1);
    arp.setPatternLength(1);
    arp.setBaseNoteSource(Arpeggiator::BaseNoteSource::External);
    arp.setBaseNote(55);
    arp.start(2);

    auto cfg = makeConfig();
    prepSlot(cfg, 2, MIDIMessageType::Note, 5, 10);

    auto pots = makePots();
    pots.potLastValues[2] = 700;

    MIDIHandler midi = primeMidi();
    arp.update(midi, cfg, pots);
    midi._clockTickCount++;
    arp.update(midi, cfg, pots);

    TEST_ASSERT_EQUAL_UINT8(55, usbMIDI.lastNoteOn);
    TEST_ASSERT_EQUAL_UINT8(55, cfg.getSlot(2).arpNote);
    TEST_ASSERT_EQUAL_UINT8(5, usbMIDI.lastNoteOnChannel);
}

void test_external_missing_inputs_falls_back_to_pot() {
    MidiUsbGuard guard;
    Arpeggiator arp;
    arp.setLength(1);
    arp.setPatternLength(1);
    arp.setBaseNoteSource(Arpeggiator::BaseNoteSource::External);
    arp.start(3);

    auto cfg = makeConfig();
    prepSlot(cfg, 3, MIDIMessageType::Note, 6, 11);

    auto pots = makePots();
    pots.potLastValues[3] = 256; // maps to 31-ish once scaled

    MIDIHandler midi = primeMidi();
    arp.update(midi, cfg, pots);
    midi._clockTickCount++;
    arp.update(midi, cfg, pots);

    TEST_ASSERT_EQUAL_UINT8(Utility::mapToMidiValue(256) % 128, usbMIDI.lastNoteOn);
    TEST_ASSERT_EQUAL_UINT8(usbMIDI.lastNoteOn, cfg.getSlot(3).arpNote);
    TEST_ASSERT_EQUAL_UINT8(6, usbMIDI.lastNoteOnChannel);
}

void test_catches_up_when_ticks_pile_up() {
    MidiUsbGuard guard;
    Arpeggiator arp;
    arp.setLength(2);
    arp.setPatternLength(4);
    arp.setBaseNoteSource(Arpeggiator::BaseNoteSource::Pot);
    arp.start(0);

    auto cfg = makeConfig();
    prepSlot(cfg, 0, MIDIMessageType::Note, 1, 0);

    auto pots = makePots();
    pots.potLastValues[0] = 0;

    MIDIHandler midi = primeMidi();
    arp.update(midi, cfg, pots); // latch the current clock snapshot

    midi._clockTickCount += 4; // pretend four ticks landed while we were away
    uint32_t beforeTx = midi._txCount;
    arp.update(midi, cfg, pots);

    TEST_ASSERT_EQUAL_UINT32(beforeTx + 2, midi._txCount); // two notes fired to catch up
    TEST_ASSERT_EQUAL_UINT8(1, usbMIDI.lastNoteOn);        // second note in the UP pattern
}
