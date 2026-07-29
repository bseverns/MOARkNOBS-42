#include <unity.h>

#include "BiquadFilter.h"
#include "EfFilterControl.h"

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
    BiquadFilter aboveNyquist;
    BiquadFilter maximumCutoff;
    aboveNyquist.configure(BiquadFilter::LOWPASS, 50000.0f, 48000.0f);
    maximumCutoff.configure(BiquadFilter::LOWPASS, 48000.0f * 0.499f, 48000.0f);

    for (int index = 0; index < 20; ++index) {
        TEST_ASSERT_FLOAT_WITHIN(0.00001f, maximumCutoff.process(1.0f),
                                 aboveNyquist.process(1.0f));
    }
}

void test_follower_step_response_uses_production_control_cadence() {
    constexpr float controlRateHz =
        efControlRateHz(static_cast<float>(EF_FOLLOWER_SCHEDULER_PERIOD_MS),
                        static_cast<float>(EF_FOLLOWERS_PER_PASS), 6.0f);
    constexpr float shapingControl = 1000.0f;
    BiquadFilter productionCadence;
    BiquadFilter legacyReference;
    productionCadence.configure(
        BiquadFilter::LOWPASS,
        efControlToCutoffHz(shapingControl, controlRateHz),
        controlRateHz);
    legacyReference.configure(BiquadFilter::LOWPASS, shapingControl,
                              EF_FILTER_REFERENCE_RATE_HZ);

    float productionOutput = 0.0f;
    float referenceOutput = 0.0f;
    for (int sample = 0; sample < 84; ++sample) {
        productionOutput = productionCadence.process(1.0f);
        referenceOutput = legacyReference.process(1.0f);
        TEST_ASSERT_FLOAT_WITHIN(0.00001f, referenceOutput, productionOutput);
        if (sample == 5) {
            TEST_ASSERT_TRUE(
                productionOutput > 0.0f && productionOutput < 0.5f);
        }
    }
    TEST_ASSERT_TRUE(productionOutput > 0.9f);
}
