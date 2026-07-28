#include <unity.h>

#include "BiquadFilter.h"

void test_lowpass_preserves_dc_on_native_host() {
    BiquadFilter filter;
    filter.configure(BiquadFilter::LOWPASS, 1000.0f, 48000.0f);

    float output = 0.0f;
    for (int index = 0; index < 1000; ++index) output = filter.process(1.0f);

    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.0f, output);
}

void test_highpass_rejects_dc_on_native_host() {
    BiquadFilter filter;
    filter.configure(BiquadFilter::HIGHPASS, 1000.0f, 48000.0f);

    float output = 0.0f;
    for (int index = 0; index < 1000; ++index) output = filter.process(1.0f);

    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, output);
}

void test_native_filter_clamps_extreme_cutoffs() {
    BiquadFilter lowCutoff;
    BiquadFilter minimumCutoff;
    lowCutoff.configure(BiquadFilter::LOWPASS, -10.0f, 48000.0f);
    minimumCutoff.configure(BiquadFilter::LOWPASS, 20.0f, 48000.0f);

    for (int index = 0; index < 20; ++index) {
        TEST_ASSERT_FLOAT_WITHIN(0.00001f, minimumCutoff.process(1.0f), lowCutoff.process(1.0f));
    }
}
