#include "unity_config.h" // yanks in Arduino and our usbMIDI doppelganger
#include <unity.h>

#define private public
#include "MIDIHandler.h"
#include "PotentiometerManager.h"
#include "Arpeggiator.h"
#undef private

#include "ConfigManager.h"
#include "Hardware/IO.h"
#include "Globals.h"
#include "TimeStub.h"
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
        slot.sysexLength = 0;
        slot.sysexTemplate.fill(0);
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

// Reset the shared scheduler so delayed tasks don't leak between tests.
void resetScheduler() { Utility::schedulerHigh = TaskScheduler(); }

void tickAndUpdate(Arpeggiator &arp, MIDIHandler &midi, ConfigManager &cfg,
                   PotentiometerManager &pots, unsigned long msPerTick, bool runScheduler = false) {
    // Advance the MIDI clock and fake time, then tick the arp.
    midi._clockTickCount++;
    advanceMs(msPerTick);
    arp.update(midi, cfg, pots);
    if (runScheduler) {
        Utility::schedulerHigh.update();
    }
}
} // namespace

void test_start_stop_cycle() {
    Arpeggiator arp;
    TEST_ASSERT_FALSE(arp.isActive());
    arp.start(7);
    TEST_ASSERT_TRUE(arp.isActive());
    TEST_ASSERT_EQUAL_UINT8(7, arp.getSlot());
    arp.stop();
    TEST_ASSERT_FALSE(arp.isActive());
}

void test_profile_assignments_gate_hardware_start_but_not_explicit_start() {
    Arpeggiator arp;
    TEST_ASSERT_FALSE(arp.isAssigned(7));
    TEST_ASSERT_FALSE(arp.startIfAssigned(7));
    TEST_ASSERT_FALSE(arp.isActive(7));

    arp.setAssigned(7, true);
    TEST_ASSERT_TRUE(arp.isAssigned(7));
    TEST_ASSERT_TRUE(arp.startIfAssigned(7));
    TEST_ASSERT_TRUE(arp.isActive(7));

    arp.stop(7);
    arp.setAssigned(7, false);
    arp.start(7);
    TEST_ASSERT_TRUE(arp.isActive(7));
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

void test_note_dynamics_shape_arp_note_velocity_and_probability() {
    MidiUsbGuard guard;
    velocityShift = -12;
    changeProbability = 100;
    g_lfoVelocityShift = 0.0f;
    g_lfoNoteChance = 0.0f;

    Arpeggiator arp;
    arp.setLength(1);
    arp.setPatternLength(1);
    arp.setBaseNoteSource(Arpeggiator::BaseNoteSource::Pot);
    arp.start(0);

    auto cfg = makeConfig();
    prepSlot(cfg, 0, MIDIMessageType::Note, 2, 10);

    auto pots = makePots();
    pots.potLastValues[0] = 1023;

    MIDIHandler midi = primeMidi();
    arp.update(midi, cfg, pots);
    midi._clockTickCount++;
    arp.update(midi, cfg, pots);

    TEST_ASSERT_EQUAL_UINT8(127, usbMIDI.lastNoteOn);
    TEST_ASSERT_EQUAL_UINT8(115, usbMIDI.lastNoteOnVelocity);
    TEST_ASSERT_EQUAL_UINT8(2, usbMIDI.lastNoteOnChannel);

    guard.reset();
    const uint32_t beforeTx = midi._txCount;
    changeProbability = 0;
    midi._clockTickCount++;
    arp.update(midi, cfg, pots);

    TEST_ASSERT_EQUAL_UINT32(beforeTx, midi._txCount);
    TEST_ASSERT_EQUAL_UINT8(0, usbMIDI.lastNoteOn);

    velocityShift = 0;
    changeProbability = 100;
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

void test_large_tick_backlog_is_bounded_and_not_replayed() {
    MidiUsbGuard guard;
    resetScheduler();

    Arpeggiator arp;
    arp.setLength(1);
    arp.setPatternLength(4);
    arp.setBaseNoteSource(Arpeggiator::BaseNoteSource::Pot);
    arp.start(0);

    auto cfg = makeConfig();
    prepSlot(cfg, 0, MIDIMessageType::Note, 1, 0);

    auto pots = makePots();
    MIDIHandler midi = primeMidi();
    arp.update(midi, cfg, pots); // latch the current clock snapshot

    midi._clockTickCount += Arpeggiator::MAX_CATCH_UP_EMISSIONS_PER_UPDATE + 32U;
    arp.update(midi, cfg, pots);

    TEST_ASSERT_EQUAL_UINT32(Arpeggiator::MAX_CATCH_UP_EMISSIONS_PER_UPDATE, midi._txCount);
    TEST_ASSERT_EQUAL_UINT(0, Utility::schedulerHigh.taskCountForTest());

    const uint32_t afterBacklogTx = midi._txCount;
    const size_t afterBacklogTasks = Utility::schedulerHigh.taskCountForTest();
    arp.update(midi, cfg, pots); // no new tick: discarded backlog must not replay

    TEST_ASSERT_EQUAL_UINT32(afterBacklogTx, midi._txCount);
    TEST_ASSERT_EQUAL_UINT(afterBacklogTasks, Utility::schedulerHigh.taskCountForTest());
}

void test_random_shape_respects_jitter_depth() {
    MidiUsbGuard guard;
    Arpeggiator arp;
    arp.setLength(1);
    arp.setPatternLength(8);
    arp.setShape(Arpeggiator::RANDOM);
    arp.setBaseNoteSource(Arpeggiator::BaseNoteSource::Slot);
    arp.start(0);

    g_jitterSettings.depth = 0.0f;
    g_jitterSettings.smoothness = 0.5f;

    auto cfg = makeConfig();
    prepSlot(cfg, 0, MIDIMessageType::Note, 1, 60);

    auto pots = makePots();
    pots.potLastValues[0] = 0;

    MIDIHandler midi = primeMidi();
    arp.update(midi, cfg, pots);
    midi._clockTickCount++;
    arp.update(midi, cfg, pots);

    TEST_ASSERT_EQUAL_UINT8(64, usbMIDI.lastNoteOn);
}

void test_updown_shape_walks_full_range() {
    MidiUsbGuard guard;
    resetScheduler();
    g_fakeNowMs = 0;
    g_tappedBPM = 120.0f;
    g_lfoArpSwing = 0.0f;

    Arpeggiator arp;
    arp.setLength(1);
    arp.setPatternLength(4);
    arp.setShape(Arpeggiator::UPDOWN);
    arp.setBaseNoteSource(Arpeggiator::BaseNoteSource::Slot);
    arp.start(0);

    auto cfg = makeConfig();
    prepSlot(cfg, 0, MIDIMessageType::Note, 1, 60);

    auto pots = makePots();
    MIDIHandler midi = primeMidi();
    arp.update(midi, cfg, pots); // latch

    // UPDOWN should climb then descend without repeating endpoints.
    uint8_t expected[] = {60, 61, 62, 63, 62, 61};
    for (size_t i = 0; i < sizeof(expected); ++i) {
        tickAndUpdate(arp, midi, cfg, pots, 20, false);
        TEST_ASSERT_EQUAL_UINT8(expected[i], usbMIDI.lastNoteOn);
    }
}

void test_drunk_shape_is_deterministic() {
    MidiUsbGuard guard;
    resetScheduler();
    g_fakeNowMs = 0;
    g_tappedBPM = 120.0f;
    g_lfoArpSwing = 0.0f;
    g_jitterSettings.depth = 1.0f;

    auto cfg = makeConfig();
    prepSlot(cfg, 0, MIDIMessageType::Note, 1, 60);
    auto pots = makePots();

    // Two separate arps with the same seed should match.
    uint8_t sequenceA[6] = {0};
    {
        Arpeggiator arp;
        arp.setLength(1);
        arp.setPatternLength(4);
        arp.setShape(Arpeggiator::DRUNK);
        arp.setBaseNoteSource(Arpeggiator::BaseNoteSource::Slot);
        arp.start(0);

        MIDIHandler midi = primeMidi();
        arp.update(midi, cfg, pots);
        for (size_t i = 0; i < 6; ++i) {
            tickAndUpdate(arp, midi, cfg, pots, 20, false);
            sequenceA[i] = usbMIDI.lastNoteOn;
        }
    }

    g_fakeNowMs = 0;
    resetScheduler();

    uint8_t sequenceB[6] = {0};
    {
        Arpeggiator arp;
        arp.setLength(1);
        arp.setPatternLength(4);
        arp.setShape(Arpeggiator::DRUNK);
        arp.setBaseNoteSource(Arpeggiator::BaseNoteSource::Slot);
        arp.start(0);

        MIDIHandler midi = primeMidi();
        arp.update(midi, cfg, pots);
        for (size_t i = 0; i < 6; ++i) {
            tickAndUpdate(arp, midi, cfg, pots, 20, false);
            sequenceB[i] = usbMIDI.lastNoteOn;
        }
    }

    for (size_t i = 0; i < 6; ++i) {
        TEST_ASSERT_EQUAL_UINT8(sequenceA[i], sequenceB[i]);
    }
}

void test_drunk_shape_jitter_depth_expands_walk() {
    MidiUsbGuard guard;
    resetScheduler();
    g_fakeNowMs = 0;
    g_tappedBPM = 120.0f;
    g_lfoArpSwing = 0.0f;
    g_jitterSettings.depth = 1.0f;

    Arpeggiator arp;
    arp.setLength(1);
    arp.setPatternLength(4);
    arp.setOctaveRange(2);
    arp.setShape(Arpeggiator::DRUNK);
    arp.setBaseNoteSource(Arpeggiator::BaseNoteSource::Slot);
    arp.start(0);

    auto cfg = makeConfig();
    prepSlot(cfg, 0, MIDIMessageType::Note, 1, 60);
    auto pots = makePots();
    MIDIHandler midi = primeMidi();
    arp.update(midi, cfg, pots);

    uint8_t lowest = 127;
    uint8_t highest = 0;
    for (uint8_t i = 0; i < 12; ++i) {
        tickAndUpdate(arp, midi, cfg, pots, 20, false);
        lowest = min(lowest, usbMIDI.lastNoteOn);
        highest = max(highest, usbMIDI.lastNoteOn);
    }

    TEST_ASSERT_TRUE(highest - lowest >= 12);
}

void test_euclidean_advances_pitch_by_hit_not_rest_position() {
    MidiUsbGuard guard;
    resetScheduler();
    g_fakeNowMs = 0;
    g_tappedBPM = 120.0f;
    g_lfoArpSwing = 0.0f;

    Arpeggiator arp;
    arp.setLength(1);
    arp.setPatternLength(4);
    arp.setOctaveRange(2);
    arp.setShape(Arpeggiator::EUCLIDEAN);
    arp.setBaseNoteSource(Arpeggiator::BaseNoteSource::Slot);
    arp.start(0);

    auto cfg = makeConfig();
    prepSlot(cfg, 0, MIDIMessageType::Note, 1, 60);
    auto pots = makePots();
    MIDIHandler midi = primeMidi();
    arp.update(midi, cfg, pots);

    uint8_t firedNotes[5] = {};
    uint8_t firedCount = 0;
    uint32_t beforeTx = midi._txCount;
    for (int i = 0; i < 12; ++i) {
        tickAndUpdate(arp, midi, cfg, pots, 20, false);
        if (midi._txCount != beforeTx) {
            beforeTx = midi._txCount;
            if (firedCount < 5) {
                firedNotes[firedCount] = usbMIDI.lastNoteOn;
            }
            ++firedCount;
        }
    }

    const uint8_t expected[] = {60, 61, 62, 63, 72};
    TEST_ASSERT_EQUAL_UINT8(5, firedCount);
    for (uint8_t i = 0; i < 5; ++i) {
        TEST_ASSERT_EQUAL_UINT8(expected[i], firedNotes[i]);
    }
}

void test_swing_delays_offbeat_notes() {
    MidiUsbGuard guard;
    resetScheduler();
    g_fakeNowMs = 0;
    g_tappedBPM = 120.0f;
    g_lfoArpSwing = 0.0f;

    Arpeggiator arp;
    arp.setLength(6);
    arp.setPatternLength(2);
    arp.setShape(Arpeggiator::UP);
    arp.setGatePercent(50.0f);
    arp.setSwingPercent(16.0f);
    arp.setBaseNoteSource(Arpeggiator::BaseNoteSource::Slot);
    arp.start(0);

    auto cfg = makeConfig();
    prepSlot(cfg, 0, MIDIMessageType::Note, 1, 60);
    auto pots = makePots();
    MIDIHandler midi = primeMidi();
    arp.update(midi, cfg, pots); // sync

    // First step: no delay, should fire immediately.
    tickAndUpdate(arp, midi, cfg, pots, 21, true);
    TEST_ASSERT_EQUAL_UINT8(60, usbMIDI.lastNoteOn);

    // Second step: offbeat should delay by ~20ms.
    uint8_t priorNote = usbMIDI.lastNoteOn;
    tickAndUpdate(arp, midi, cfg, pots, 21, false);
    TEST_ASSERT_EQUAL_UINT8(priorNote, usbMIDI.lastNoteOn);
    advanceMs(19);
    arp.update(midi, cfg, pots);
    TEST_ASSERT_EQUAL_UINT8(priorNote, usbMIDI.lastNoteOn);
    advanceMs(2);
    arp.update(midi, cfg, pots);
    TEST_ASSERT_EQUAL_UINT8(61, usbMIDI.lastNoteOn);
}

void test_stop_cancels_delayed_note_on() {
    MidiUsbGuard guard;
    resetScheduler();
    g_fakeNowMs = 0;
    g_tappedBPM = 120.0f;
    g_lfoArpSwing = 0.0f;

    Arpeggiator arp;
    arp.setLength(6);
    arp.setPatternLength(2);
    arp.setShape(Arpeggiator::UP);
    arp.setGatePercent(50.0f);
    arp.setSwingPercent(16.0f);
    arp.setBaseNoteSource(Arpeggiator::BaseNoteSource::Slot);
    arp.start(0);

    auto cfg = makeConfig();
    prepSlot(cfg, 0, MIDIMessageType::Note, 1, 60);
    auto pots = makePots();
    MIDIHandler midi = primeMidi();
    arp.update(midi, cfg, pots);
    tickAndUpdate(arp, midi, cfg, pots, 21, true); // first, immediate note
    const uint8_t priorNote = usbMIDI.lastNoteOn;

    tickAndUpdate(arp, midi, cfg, pots, 21, false); // second, delayed by swing
    arp.stop(0);
    advanceMs(50);
    arp.update(midi, cfg, pots);

    TEST_ASSERT_EQUAL_UINT8(priorNote, usbMIDI.lastNoteOn);
}

void test_tempo_change_updates_tick_ms() {
    resetScheduler();
    g_fakeNowMs = 0;
    g_tappedBPM = 0.0f;
    g_lfoArpSwing = 0.0f;

    Arpeggiator arp;
    arp.setLength(1);
    arp.setPatternLength(2);
    arp.setShape(Arpeggiator::UP);
    arp.setBaseNoteSource(Arpeggiator::BaseNoteSource::Slot);
    arp.start(0);

    auto cfg = makeConfig();
    prepSlot(cfg, 0, MIDIMessageType::Note, 1, 60);
    auto pots = makePots();
    MIDIHandler midi = primeMidi();
    arp.update(midi, cfg, pots);

    // Prime ms-per-tick at ~20 ms.
    for (int i = 0; i < 4; ++i) {
        tickAndUpdate(arp, midi, cfg, pots, 20, false);
    }
    float first = arp.msPerTickEstimate();
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 20.0f, first);

    // Simulate a tempo increase (10 ms per tick) and verify smoothing reacts.
    for (int i = 0; i < 4; ++i) {
        tickAndUpdate(arp, midi, cfg, pots, 10, false);
    }
    TEST_ASSERT_TRUE(arp.msPerTickEstimate() < first);
}
