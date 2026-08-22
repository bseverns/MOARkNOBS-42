#include "unity_config.h"
#include <unity.h>

#include "MIDIHandler.h"
#include "MidiInputRouter.h"

namespace {
struct AppliedWrite {
    MachineParameterTarget target = MachineParameterTarget::SlotValue;
    uint8_t index = 0;
    uint8_t value = 0;
    MidiInputPort port = MidiInputPort::Any;
};

AppliedWrite lastWrite{};
uint8_t writeCount = 0;
uint8_t currentValue = 64;
uint8_t midiCallbackCount = 0;
MidiInputPort lastMidiPort = MidiInputPort::Any;

bool recordApply(MachineParameterTarget target, uint8_t index, uint8_t value,
                 MidiInputPort port) {
    lastWrite = {target, index, value, port};
    ++writeCount;
    currentValue = value;
    return true;
}

bool readCurrent(MachineParameterTarget, uint8_t, uint8_t &value) {
    value = currentValue;
    return true;
}

void recordMidiCc(MidiInputPort port, uint8_t, uint8_t, uint8_t) {
    lastMidiPort = port;
    ++midiCallbackCount;
}

MidiInputBinding bindingFor(uint8_t controller, MachineParameterTarget target) {
    MidiInputBinding binding{};
    binding.channel = 3;
    binding.controller = controller;
    binding.target = static_cast<uint8_t>(target);
    binding.flags = 0;
    return binding;
}
} // namespace

void test_midi_input_router_matches_port_channel_and_controller() {
    MidiInputRouter router;
    router.setCallbacks(recordApply, readCurrent);
    MidiInputBinding binding = bindingFor(74, MachineParameterTarget::ArpSwing);
    binding.port = static_cast<uint8_t>(MidiInputPort::Din);
    router.setBindings(&binding, 1);
    writeCount = 0;

    TEST_ASSERT_EQUAL_UINT8(0, router.routeControlChange(MidiInputPort::Usb, 3, 74, 90));
    TEST_ASSERT_EQUAL_UINT8(0, router.routeControlChange(MidiInputPort::Din, 2, 74, 90));
    TEST_ASSERT_EQUAL_UINT8(0, router.routeControlChange(MidiInputPort::Din, 3, 73, 90));
    TEST_ASSERT_EQUAL_UINT8(1, router.routeControlChange(MidiInputPort::Din, 3, 74, 90));
    TEST_ASSERT_EQUAL_UINT8(1, writeCount);
    TEST_ASSERT_EQUAL_UINT8(90, lastWrite.value);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(MidiInputPort::Din),
                            static_cast<uint8_t>(lastWrite.port));
}

void test_midi_input_router_soft_pickup_waits_for_crossing() {
    MidiInputRouter router;
    router.setCallbacks(recordApply, readCurrent);
    MidiInputBinding binding = bindingFor(10, MachineParameterTarget::JitterDepth);
    binding.flags = MIDI_INPUT_FLAG_SOFT_TAKEOVER;
    router.setBindings(&binding, 1);
    currentValue = 64;
    writeCount = 0;

    TEST_ASSERT_EQUAL_UINT8(0, router.routeControlChange(MidiInputPort::Din, 3, 10, 12));
    TEST_ASSERT_EQUAL_UINT8(0, router.routeControlChange(MidiInputPort::Din, 3, 10, 50));
    TEST_ASSERT_EQUAL_UINT8(1, router.routeControlChange(MidiInputPort::Din, 3, 10, 70));
    TEST_ASSERT_EQUAL_UINT8(1, router.routeControlChange(MidiInputPort::Din, 3, 10, 80));
    TEST_ASSERT_EQUAL_UINT8(2, writeCount);
    TEST_ASSERT_EQUAL_UINT8(80, lastWrite.value);
}

void test_midi_input_router_soft_pickup_rearms_after_external_target_move() {
    MidiInputRouter router;
    router.setCallbacks(recordApply, readCurrent);
    MidiInputBinding binding = bindingFor(11, MachineParameterTarget::JitterSmoothness);
    binding.flags = MIDI_INPUT_FLAG_SOFT_TAKEOVER;
    router.setBindings(&binding, 1);
    currentValue = 64;
    writeCount = 0;

    TEST_ASSERT_EQUAL_UINT8(1, router.routeControlChange(MidiInputPort::Din, 3, 11, 64));
    currentValue = 100;
    TEST_ASSERT_EQUAL_UINT8(0, router.routeControlChange(MidiInputPort::Din, 3, 11, 70));
    TEST_ASSERT_EQUAL_UINT8(1, router.routeControlChange(MidiInputPort::Din, 3, 11, 110));
    TEST_ASSERT_EQUAL_UINT8(2, writeCount);
}

void test_midi_input_router_toggle_fires_only_on_rising_edges() {
    MidiInputRouter router;
    router.setCallbacks(recordApply, readCurrent);
    MidiInputBinding binding = bindingFor(20, MachineParameterTarget::NoteChance);
    binding.mode = static_cast<uint8_t>(MidiInputMode::Toggle);
    binding.minValue = 10;
    binding.maxValue = 100;
    router.setBindings(&binding, 1);
    writeCount = 0;

    TEST_ASSERT_EQUAL_UINT8(1, router.routeControlChange(MidiInputPort::Usb, 3, 20, 127));
    TEST_ASSERT_EQUAL_UINT8(0, router.routeControlChange(MidiInputPort::Usb, 3, 20, 127));
    TEST_ASSERT_EQUAL_UINT8(0, router.routeControlChange(MidiInputPort::Usb, 3, 20, 0));
    TEST_ASSERT_EQUAL_UINT8(1, router.routeControlChange(MidiInputPort::Usb, 3, 20, 127));
    TEST_ASSERT_EQUAL_UINT8(2, writeCount);
    TEST_ASSERT_EQUAL_UINT8(10, lastWrite.value);
}

void test_midi_handler_forwards_only_plain_cc_with_origin() {
    MIDIHandler handler;
    handler.setControlChangeInputCallback(recordMidiCc);
    midiCallbackCount = 0;
    lastMidiPort = MidiInputPort::Any;

    handler.handleMIDI(midi::ControlChange, 1, 99, 1, MidiInputPort::Din);
    handler.handleMIDI(midi::ControlChange, 1, 98, 2, MidiInputPort::Din);
    handler.handleMIDI(midi::ControlChange, 1, 6, 3, MidiInputPort::Din);
    handler.handleMIDI(midi::ControlChange, 1, 38, 4, MidiInputPort::Din);
    TEST_ASSERT_EQUAL_UINT8(0, midiCallbackCount);

    handler.handleMIDI(midi::ControlChange, 1, 74, 99, MidiInputPort::Usb);
    TEST_ASSERT_EQUAL_UINT8(1, midiCallbackCount);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(MidiInputPort::Usb),
                            static_cast<uint8_t>(lastMidiPort));
}
