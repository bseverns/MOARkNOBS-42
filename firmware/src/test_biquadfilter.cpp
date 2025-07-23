#include <Arduino.h>
#include "BiquadFilter.h"

// Standalone unit test for the BiquadFilter class. Used during development to
// verify coefficient calculations and stability outside of the full firmware.
// - confirms filter state behaves as expected

void setup() {
    Serial.begin(115200);
    while(!Serial) { /* wait for serial */ }
    Serial.println("=== BiquadFilter Test ===");

    const float freq = 1000.0f;
    const float sampleRate = 48000.0f;
    const float q = 0.707f;

    bool passLP = false;
    bool passHP = false;
    bool passBP = false;

    auto runTest = [&](BiquadFilter::FilterType type) -> bool {
        BiquadFilter filter;
        filter.configure(type, freq, sampleRate, q);

        float omega = 2.0f * PI * freq / sampleRate;
        float cos_omega = cosf(omega);
        float sin_omega = sinf(omega);
        float alpha = sin_omega / (2.0f * q);

        float a0 = 0.0f;
        float a1 = 0.0f;
        float a2 = 0.0f;
        float b1 = -2.0f * cos_omega;
        float b2 = 1.0f - alpha;

        switch(type) {
            case BiquadFilter::LOWPASS:
                a0 = (1.0f - cos_omega) / 2.0f;
                a1 = 1.0f - cos_omega;
                a2 = a0;
                break;
            case BiquadFilter::HIGHPASS:
                a0 = (1.0f + cos_omega) / 2.0f;
                a1 = -(1.0f + cos_omega);
                a2 = a0;
                break;
            case BiquadFilter::BANDPASS:
                a0 = alpha;
                a1 = 0.0f;
                a2 = -alpha;
                break;
        }

        float norm = 1.0f + alpha;
        a0 /= norm; a1 /= norm; a2 /= norm; b1 /= norm; b2 /= norm;

        float firstOut = filter.process(1.0f);
        bool coefCheck = fabs(firstOut - a0) < 1e-6f;

        for (int i = 0; i < 50; ++i) {
            filter.process(0.0f);
        }
        bool settleCheck = fabs(filter.process(0.0f)) < 1e-3f;

        return coefCheck && settleCheck;
    };

    passLP = runTest(BiquadFilter::LOWPASS);
    passHP = runTest(BiquadFilter::HIGHPASS);
    passBP = runTest(BiquadFilter::BANDPASS);

    if (passLP && passHP && passBP) {
        Serial.println("BiquadFilter tests PASS");
    } else {
        Serial.println("BiquadFilter tests FAIL");
    }
}

void loop() {}

