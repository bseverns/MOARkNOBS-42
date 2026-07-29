#ifndef BIQUAD_FILTER_H
#define BIQUAD_FILTER_H

#include <math.h>

/*
Lightweight biquad filter used by the envelope follower.

The filter can operate in low‑pass, high‑pass or band‑pass mode and
exposes a very small API for run‑time configuration.  It is intended
for real‑time audio/control‑rate processing where only single sample
values are processed.
*/
class BiquadFilter {
  public:
    /*
    Supported filter response types.
    */
    enum FilterType {
        LOWPASS,  // Allows frequencies below the cutoff to pass
        HIGHPASS, // Allows frequencies above the cutoff to pass
        BANDPASS  // Allows a band around the cutoff to pass
    };

    BiquadFilter() : a0(0), a1(0), a2(0), b1(0), b2(0), x1(0), x2(0), y1(0), y2(0) {}

    /*
    Configure the filter coefficients.

    The coefficients are calculated according to the selected
    filter type. Frequency is clamped to the valid range for the supplied
    sample rate before coefficients are computed. Calling this wipes
    any previously stored samples so the filter starts fresh.

    - type: Desired filter type.
    - frequency: Cutoff/centre frequency in Hz.
    - sampleRate: Sampling rate of the incoming data in Hz.
    - q: Resonance/Q value (default is 0.707).
    */
    void configure(FilterType type, float frequency, float sampleRate, float q = 0.707) {
        // Keep this DSP primitive independent of Arduino so its coefficient
        // math can also be tested on a native host target.
        if (sampleRate <= 0.0f) sampleRate = 1.0f;
        const float minimumFrequency = sampleRate * 0.000001f;
        const float maximumFrequency = sampleRate * 0.499f;
        if (frequency < minimumFrequency) frequency = minimumFrequency;
        if (frequency > maximumFrequency) frequency = maximumFrequency;

        constexpr float kPi = 3.14159265358979323846f;
        float omega = 2.0f * kPi * frequency / sampleRate;
        float cos_omega = cosf(omega);
        float sin_omega = sinf(omega);
        float alpha = sin_omega / (2.0f * q);

        switch (type) {
        case LOWPASS:
            a0 = (1 - cos_omega) / 2.0f;
            a1 = 1 - cos_omega;
            a2 = (1 - cos_omega) / 2.0f;
            b1 = -2.0f * cos_omega;
            b2 = 1.0f - alpha;
            break;
        case HIGHPASS:
            a0 = (1 + cos_omega) / 2.0f;
            a1 = -(1 + cos_omega);
            a2 = (1 + cos_omega) / 2.0f;
            b1 = -2.0f * cos_omega;
            b2 = 1.0f - alpha;
            break;
        case BANDPASS:
            a0 = alpha;
            a1 = 0.0f;
            a2 = -alpha;
            b1 = -2.0f * cos_omega;
            b2 = 1.0f - alpha;
            break;
        }

        float norm = 1.0f + alpha;
        a0 /= norm;
        a1 /= norm;
        a2 /= norm;
        b1 /= norm;
        b2 /= norm;

        // Reset internal state so old history doesn't haunt new settings
        x1 = x2 = y1 = y2 = 0.0f;
    }

    /*
    Process a single input sample.

    - input: New sample to be filtered.
    Returns Filtered sample value.
    */
    float process(float input) {
        float output = a0 * input + a1 * x1 + a2 * x2 - b1 * y1 - b2 * y2;
        x2 = x1;
        x1 = input;
        y2 = y1;
        y1 = output;
        return output;
    }

  private:
    float a0, a1, a2;
    float b1, b2;
    float x1, x2; // Previous input samples
    float y1, y2; // Previous output samples
};

#endif // BIQUAD_FILTER_H
