#include <Arduino.h>
#include "BiquadFilter.h"

// Basic DSP sanity checks for BiquadFilter
// - verifies low‑pass coefficients
// - confirms filter state behaves as expected

void setup() {
    Serial.begin(115200);
    while(!Serial) { /* wait for serial */ }
    Serial.println("=== BiquadFilter Test ===");

    const float freq = 1000.0f;
    const float sampleRate = 48000.0f;
    const float q = 0.707f;

    BiquadFilter filter;
    filter.configure(BiquadFilter::LOWPASS, freq, sampleRate, q);

    float omega = 2.0f * PI * freq / sampleRate;
    float cos_omega = cosf(omega);
    float sin_omega = sinf(omega);
    float alpha = sin_omega / (2.0f * q);

    float a0 = (1.0f - cos_omega) / 2.0f;
    float a1 = 1.0f - cos_omega;
    float a2 = a0;
    float b1 = -2.0f * cos_omega;
    float b2 = 1.0f - alpha;
    float norm = 1.0f + alpha;
    a0 /= norm; a1 /= norm; a2 /= norm; b1 /= norm; b2 /= norm;

    float out1 = filter.process(1.0f);
    bool coefCheck = fabs(out1 - a0) < 1e-6f;

    // Let the filter settle with zeros
    for (int i = 0; i < 50; ++i) {
        filter.process(0.0f);
    }
    bool settleCheck = fabs(filter.process(0.0f)) < 1e-3f;

    // Reconfigure to make sure coefficients actually change
    filter.configure(BiquadFilter::LOWPASS, 5000.0f, sampleRate, q);
    float out2 = filter.process(1.0f);
    bool updateCheck = fabs(out1 - out2) > 1e-4f;

    if (coefCheck && settleCheck && updateCheck) {
        Serial.println("BiquadFilter tests PASS");
    } else {
        Serial.println("BiquadFilter tests FAIL");
    }
}

void loop() {}
