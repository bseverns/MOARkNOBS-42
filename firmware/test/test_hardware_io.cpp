#include "unity_config.h"
#include <unity.h>

#include <vector>

#include "ButtonManager.h"
#include "Hardware/IO.h"
#include "Globals.h"

namespace {

// This file is our sandbox for the hardware::IO indirection layer.  By hot-
// swapping providers we emulate analog/digital readings without touching real
// silicon and prove the shims behave like the live firmware.

int stubAnalogA(uint8_t) { return 111; }
int stubAnalogB(uint8_t) { return 222; }

int stubDigitalHigh(uint8_t) { return HIGH; }
int stubDigitalLow(uint8_t) { return LOW; }

struct AnalogSequence {
    const int *values;
    size_t size;
    size_t index;
};

AnalogSequence sequence{nullptr, 0, 0};

int sequenceAnalog(uint8_t) {
    if (!sequence.values || sequence.size == 0) {
        return 0;
    }
    int value = sequence.values[sequence.index % sequence.size];
    sequence.index++;
    return value;
}

class ScopedSequence {
  public:
    explicit ScopedSequence(const std::initializer_list<int> &vals) : guard_(sequenceAnalog) {
        buffer_.assign(vals.begin(), vals.end());
        sequence.values = buffer_.data();
        sequence.size = buffer_.size();
        sequence.index = 0;
    }

    ~ScopedSequence() {
        sequence.values = nullptr;
        sequence.size = 0;
        sequence.index = 0;
    }

  private:
    std::vector<int> buffer_;
    hardware::ScopedAnalogReadProvider guard_;
};

} // namespace

// Nesting ScopedAnalogReadProvider instances should restore the previous
// callback once the inner guard dies.  This verifies the stack discipline.
void test_scoped_analog_provider_nesting() {
    hardware::ScopedAnalogReadProvider outer(stubAnalogA);
    TEST_ASSERT_EQUAL(111, hardware::readAnalog(0));
    {
        hardware::ScopedAnalogReadProvider inner(stubAnalogB);
        TEST_ASSERT_EQUAL(222, hardware::readAnalog(0));
    }
    TEST_ASSERT_EQUAL(111, hardware::readAnalog(0));
}

// Swap in a deterministic analog sequence and make sure the mux reader cycles
// through values like a firmware scan would.
void test_sequence_provider_cycles_values() {
    ScopedSequence values{0, 512, 1023};
    TEST_ASSERT_EQUAL(0, hardware::readAnalog(0));
    TEST_ASSERT_EQUAL(512, hardware::readAnalog(0));
    TEST_ASSERT_EQUAL(1023, hardware::readAnalog(0));
    TEST_ASSERT_EQUAL(0, hardware::readAnalog(0));
}

// setAnalogReadProvider() should hand you the previous callback so callers can
// unwind gracefully.  This covers that handshake.
void test_set_provider_returns_previous() {
    auto baseline = hardware::currentAnalogReadProvider();
    auto previous = hardware::setAnalogReadProvider(stubAnalogA);
    TEST_ASSERT_EQUAL_PTR(baseline, previous);
    TEST_ASSERT_EQUAL(111, hardware::readAnalog(0));
    auto swapped = hardware::setAnalogReadProvider(stubAnalogB);
    TEST_ASSERT_EQUAL_PTR(stubAnalogA, swapped);
    TEST_ASSERT_EQUAL(222, hardware::readAnalog(0));
    hardware::setAnalogReadProvider(baseline);
}

// Digital reads also funnel through the provider API.  Force HIGH/LOW values
// and make sure ButtonManager reports the expected active-low truth table.
void test_digital_provider_overrides_matrix_reads() {
    const uint8_t controlPins[NUM_CONTROL_BUTTONS] = {0, 1, 2, 3, 4, 5};
    ButtonManager manager(hwConfig, controlPins, nullptr);
    hardware::ScopedDigitalReadProvider hi(stubDigitalLow);
    TEST_ASSERT_TRUE(manager.readControlButtonForTest(0));
    hardware::ScopedDigitalReadProvider lo(stubDigitalHigh);
    TEST_ASSERT_FALSE(manager.readControlButtonForTest(1));
}

#if defined(UNIT_TEST) && !defined(UNIT_TEST_PROTOCOL_IMPL)

ButtonManager::ButtonManager(const HardwareConfig &config, const uint8_t *controlPins,
                             PotentiometerManager *potentiometerManager)
    : _cfg(config), _controlPins(controlPins), _potentiometerManager(potentiometerManager),
      activeMode(0), _pendingEfSlot(-1), _efAssignDeadline(0) {
    for (size_t i = 0; i < NUM_VIRTUAL_BUTTONS + NUM_CONTROL_BUTTONS; ++i) {
        buttonStates[i] = false;
        lastDebounceTimes[i] = 0;
    }
}

bool ButtonManager::readControlButton(uint8_t buttonIndex) {
    return hardware::readDigital(_controlPins[buttonIndex]) == LOW;
}

#endif // defined(UNIT_TEST)
