#include <Arduino.h>
#include <unity.h>
#include "BiquadFilter.h"

void test_frequency_clamps_to_20hz() {
    const float sampleRate = 48000.0f;
    BiquadFilter filtered_low;
    BiquadFilter filtered_20;
    filtered_low.configure(BiquadFilter::LOWPASS, 10.0f, sampleRate);
    filtered_20.configure(BiquadFilter::LOWPASS, 20.0f, sampleRate);

    float input = 0.5f;
    for (int i = 0; i < 5; ++i) {
        float out_low = filtered_low.process(input);
        float out_20  = filtered_20.process(input);
        TEST_ASSERT_FLOAT_WITHIN(0.0001f, out_20, out_low);
    }
}

void setup() {
    UNITY_BEGIN();
    RUN_TEST(test_frequency_clamps_to_20hz);
    UNITY_END();
}

void loop() {}
