#include <unity.h>

#include "MidiInputRouter.h"

namespace {
struct AppliedWrite {
    MachineParameterTarget target = MachineParameterTarget::SlotValue;
    uint8_t index = 0;
    uint8_t value = 0;
    MidiInputPort port = MidiInputPort::Any;
};

AppliedWrite writes[4]{};
uint8_t writeCount = 0;
uint8_t currentValue = 64;
bool acceptWrites = true;

bool recordApply(MachineParameterTarget target, uint8_t index, uint8_t value,
                 MidiInputPort port) {
    if (writeCount < 4) writes[writeCount] = {target, index, value, port};
    ++writeCount;
    if (acceptWrites) currentValue = value;
    return acceptWrites;
}

bool readCurrent(MachineParameterTarget, uint8_t, uint8_t &value) {
    value = currentValue;
    return true;
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

void test_router_matches_port_channel_and_controller() {
    MidiInputRouter router;
    router.setCallbacks(recordApply, readCurrent);
    MidiInputBinding binding = bindingFor(74, MachineParameterTarget::ArpSwing);
    binding.port = static_cast<uint8_t>(MidiInputPort::Din);
    router.setBindings(&binding, 1);
    writeCount = 0;
    acceptWrites = true;

    TEST_ASSERT_EQUAL_UINT8(0, router.routeControlChange(MidiInputPort::Usb, 3, 74, 90));
    TEST_ASSERT_EQUAL_UINT8(0, router.routeControlChange(MidiInputPort::Din, 2, 74, 90));
    TEST_ASSERT_EQUAL_UINT8(0, router.routeControlChange(MidiInputPort::Din, 3, 73, 90));
    TEST_ASSERT_EQUAL_UINT8(1, router.routeControlChange(MidiInputPort::Din, 3, 74, 90));
    TEST_ASSERT_EQUAL_UINT8(1, writeCount);
    TEST_ASSERT_EQUAL_UINT8(90, writes[0].value);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(MidiInputPort::Din),
                            static_cast<uint8_t>(writes[0].port));
}

void test_router_soft_pickup_waits_for_crossing() {
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
    TEST_ASSERT_EQUAL_UINT8(80, writes[1].value);
}

void test_router_soft_pickup_rearms_after_external_target_move() {
    MidiInputRouter router;
    router.setCallbacks(recordApply, readCurrent);
    MidiInputBinding binding = bindingFor(11, MachineParameterTarget::JitterSmoothness);
    binding.flags = MIDI_INPUT_FLAG_SOFT_TAKEOVER;
    router.setBindings(&binding, 1);
    currentValue = 64;
    writeCount = 0;

    TEST_ASSERT_EQUAL_UINT8(1, router.routeControlChange(MidiInputPort::Din, 3, 11, 64));
    currentValue = 100; // Physical/other-controller move after pickup was acquired.
    TEST_ASSERT_EQUAL_UINT8(0, router.routeControlChange(MidiInputPort::Din, 3, 11, 70));
    TEST_ASSERT_EQUAL_UINT8(1, router.routeControlChange(MidiInputPort::Din, 3, 11, 110));
    TEST_ASSERT_EQUAL_UINT8(2, writeCount);
}

void test_router_toggle_fires_only_on_rising_edges() {
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
    TEST_ASSERT_EQUAL_UINT8(10, writes[1].value);
}

void test_router_toggle_retries_same_value_after_failed_apply() {
    MidiInputRouter router;
    router.setCallbacks(recordApply, readCurrent);
    MidiInputBinding binding = bindingFor(22, MachineParameterTarget::SlotValue);
    binding.mode = static_cast<uint8_t>(MidiInputMode::Toggle);
    binding.minValue = 10;
    binding.maxValue = 100;
    router.setBindings(&binding, 1);
    writeCount = 0;
    acceptWrites = false;

    TEST_ASSERT_EQUAL_UINT8(0, router.routeControlChange(MidiInputPort::Usb, 3, 22, 127));
    TEST_ASSERT_EQUAL_UINT8(100, writes[0].value);
    TEST_ASSERT_EQUAL_UINT8(0, router.routeControlChange(MidiInputPort::Usb, 3, 22, 0));
    acceptWrites = true;
    TEST_ASSERT_EQUAL_UINT8(1, router.routeControlChange(MidiInputPort::Usb, 3, 22, 127));
    TEST_ASSERT_EQUAL_UINT8(100, writes[1].value);
    TEST_ASSERT_EQUAL_UINT8(2, writeCount);
}

void test_router_supports_multiple_destinations_for_one_source() {
    MidiInputRouter router;
    router.setCallbacks(recordApply, readCurrent);
    MidiInputBinding bindings[2] = {
        bindingFor(21, MachineParameterTarget::ArpSwing),
        bindingFor(21, MachineParameterTarget::ArpGate),
    };
    router.setBindings(bindings, 2);
    writeCount = 0;

    TEST_ASSERT_EQUAL_UINT8(2, router.routeControlChange(MidiInputPort::Din, 3, 21, 100));
    TEST_ASSERT_EQUAL_UINT8(2, writeCount);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(MachineParameterTarget::ArpSwing),
                            static_cast<uint8_t>(writes[0].target));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(MachineParameterTarget::ArpGate),
                            static_cast<uint8_t>(writes[1].target));
}
