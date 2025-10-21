#include "unity_config.h"
#include <unity.h>

#include <vector>

#include "ButtonManager.h"
#include "Hardware/IO.h"
#include "Globals.h"

namespace {

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

void test_scoped_analog_provider_nesting() {
    hardware::ScopedAnalogReadProvider outer(stubAnalogA);
    TEST_ASSERT_EQUAL(111, hardware::readAnalog(0));
    {
        hardware::ScopedAnalogReadProvider inner(stubAnalogB);
        TEST_ASSERT_EQUAL(222, hardware::readAnalog(0));
    }
    TEST_ASSERT_EQUAL(111, hardware::readAnalog(0));
}

void test_sequence_provider_cycles_values() {
    ScopedSequence values{0, 512, 1023};
    TEST_ASSERT_EQUAL(0, hardware::readAnalog(0));
    TEST_ASSERT_EQUAL(512, hardware::readAnalog(0));
    TEST_ASSERT_EQUAL(1023, hardware::readAnalog(0));
    TEST_ASSERT_EQUAL(0, hardware::readAnalog(0));
}

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

void test_digital_provider_overrides_matrix_reads() {
    const uint8_t controlPins[NUM_CONTROL_BUTTONS] = {0, 1, 2, 3, 4, 5};
    ButtonManager manager(hwConfig, controlPins, nullptr);
    hardware::ScopedDigitalReadProvider hi(stubDigitalLow);
    TEST_ASSERT_TRUE(manager.readControlButtonForTest(0));
    hardware::ScopedDigitalReadProvider lo(stubDigitalHigh);
    TEST_ASSERT_FALSE(manager.readControlButtonForTest(1));
}
