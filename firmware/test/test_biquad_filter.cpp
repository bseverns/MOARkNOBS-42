#include "unity_config.h" // bundles Arduino and the usbMIDI stand-in
#include <unity.h>
#include "BiquadFilter.h"

void test_lowpass_highpass_response() {
    BiquadFilter lp;
    lp.configure(BiquadFilter::LOWPASS, 1000.0f, 48000.0f);
    float lpOut = 0.0f;
    for (int i = 0; i < 20; ++i) {
        lpOut = lp.process(1.0f);
    }
    BiquadFilter hp;
    hp.configure(BiquadFilter::HIGHPASS, 1000.0f, 48000.0f);
    float hpOut = 0.0f;
    // Blast it with a step for 20 samples, then feed 50 zeros so the high-pass can settle
    for (int i = 0; i < 20; ++i) {
        hp.process(1.0f);
    }
    for (int i = 0; i < 50; ++i) {
        hpOut = hp.process(0.0f);
    }
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 1.0f, lpOut);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 0.0f, hpOut);
}
