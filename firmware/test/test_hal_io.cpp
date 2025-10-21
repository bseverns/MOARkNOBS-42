#include "unity_config.h"
#include <unity.h>

#include "hal/RuntimeIO.h"

#include <map>
#include <vector>

namespace {
struct AnalogSequence {
    std::vector<int> values;
    size_t index = 0;
};

int analogSequence(uint8_t, void *ctx) {
    auto *sequence = static_cast<AnalogSequence *>(ctx);
    if (sequence->index >= sequence->values.size()) {
        return -1;
    }
    return sequence->values[sequence->index++];
}

struct DigitalScenario {
    std::map<uint8_t, int> values;
};

int digitalSequence(uint8_t pin, void *ctx) {
    auto *scenario = static_cast<DigitalScenario *>(ctx);
    auto it = scenario->values.find(pin);
    if (it == scenario->values.end()) {
        return HIGH;
    }
    return it->second;
}

struct TimeSequence {
    std::vector<unsigned long> values;
    size_t index = 0;
};

unsigned long timeSequence(void *ctx) {
    auto *sequence = static_cast<TimeSequence *>(ctx);
    if (sequence->index >= sequence->values.size()) {
        return sequence->values.empty() ? 0UL : sequence->values.back();
    }
    return sequence->values[sequence->index++];
}
} // namespace

void test_hal_analog_override_sequence() {
    moar::hal::clearAnalogReadHook();
    AnalogSequence seq{{101, 202, 303}};
    {
        moar::hal::ScopedAnalogReadHook hook{analogSequence, &seq};
        TEST_ASSERT_EQUAL(101, moar::hal::readAnalog(0));
        TEST_ASSERT_EQUAL(202, moar::hal::readAnalog(1));
        TEST_ASSERT_EQUAL(303, moar::hal::readAnalog(2));
    }
}

void test_hal_analog_nested_scopes_restore_previous() {
    moar::hal::clearAnalogReadHook();
    AnalogSequence outer{{11, 12}};
    AnalogSequence inner{{99}};
    {
        moar::hal::ScopedAnalogReadHook outerHook{analogSequence, &outer};
        TEST_ASSERT_EQUAL(11, moar::hal::readAnalog(5));
        {
            moar::hal::ScopedAnalogReadHook innerHook{analogSequence, &inner};
            TEST_ASSERT_EQUAL(99, moar::hal::readAnalog(5));
        }
        TEST_ASSERT_EQUAL(12, moar::hal::readAnalog(5));
    }
    moar::hal::clearAnalogReadHook();
}

void test_hal_digital_override() {
    moar::hal::clearDigitalReadHook();
    DigitalScenario scenario{{{4, LOW}, {7, HIGH}}};
    moar::hal::ScopedDigitalReadHook hook{digitalSequence, &scenario};
    TEST_ASSERT_EQUAL(LOW, moar::hal::readDigital(4));
    TEST_ASSERT_EQUAL(HIGH, moar::hal::readDigital(7));
    TEST_ASSERT_EQUAL(HIGH, moar::hal::readDigital(13));
}

void test_hal_time_hooks() {
    moar::hal::clearMillisHook();
    moar::hal::clearMicrosHook();
    TimeSequence millisSeq{{10, 20, 40}};
    TimeSequence microsSeq{{5, 15}};
    {
        moar::hal::ScopedMillisHook hook{timeSequence, &millisSeq};
        TEST_ASSERT_EQUAL_UINT32(10, moar::hal::getMillis());
        TEST_ASSERT_EQUAL_UINT32(20, moar::hal::getMillis());
        TEST_ASSERT_EQUAL_UINT32(40, moar::hal::getMillis());
        TEST_ASSERT_EQUAL_UINT32(40, moar::hal::getMillis());
    }
    {
        moar::hal::ScopedMicrosHook hook{timeSequence, &microsSeq};
        TEST_ASSERT_EQUAL_UINT32(5, moar::hal::getMicros());
        TEST_ASSERT_EQUAL_UINT32(15, moar::hal::getMicros());
        TEST_ASSERT_EQUAL_UINT32(15, moar::hal::getMicros());
    }
}

void test_hal_hook_roundtrip_metadata() {
    AnalogSequence seq{{7}};
    moar::hal::setAnalogReadHook(analogSequence, &seq);
    auto hook = moar::hal::getAnalogReadHook();
    TEST_ASSERT_EQUAL_PTR(reinterpret_cast<void *>(analogSequence), reinterpret_cast<void *>(hook.fn));
    TEST_ASSERT_EQUAL_PTR(&seq, hook.ctx);
    moar::hal::clearAnalogReadHook();
}
