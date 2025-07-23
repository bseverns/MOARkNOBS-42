#ifndef BIQUAD_FILTER_H
#define BIQUAD_FILTER_H

/**
 * @brief Lightweight biquad filter used by the Envelope Follower.
 *
 * The filter can operate in low‑pass, high‑pass or band‑pass mode and
 * exposes a very small API for run‑time configuration.  It is intended
 * for real‑time audio/control‑rate processing where only single sample
 * values are processed.
 */
class BiquadFilter {
public:
    /**
     * @brief Supported filter response types.
     */
    enum FilterType {
        LOWPASS,   //!< Allows frequencies below the cutoff to pass
        HIGHPASS,  //!< Allows frequencies above the cutoff to pass
        BANDPASS   //!< Allows a band around the cutoff to pass
    };

    BiquadFilter()
        : a0(0), a1(0), a2(0), b1(0), b2(0),
          x1(0), x2(0), y1(0), y2(0) {}

    /**
     * @brief Configure the filter coefficients.
     *
     * The coefficients are calculated according to the selected
     * filter type.  Frequency values outside of the audible range are
     * clamped before the coefficients are computed.
     *
     * @param type        Desired filter type.
     * @param frequency   Cutoff/centre frequency in Hz.
     * @param sampleRate  Sampling rate of the incoming data in Hz.
     * @param q           Resonance/Q value (default is 0.707).
     */
    void configure(FilterType type, float frequency, float sampleRate, float q = 0.707) {
        // Constrain frequency to a valid range (e.g., 20 Hz to 20 kHz)
        frequency = constrain(frequency, 20.0f, 20000.0f);

        float omega = 2.0f * PI * frequency / sampleRate;
        float cos_omega = cos(omega);
        float sin_omega = sin(omega);
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
    }

    /**
     * @brief Process a single input sample.
     *
     * @param input  New sample to be filtered.
     * @return Filtered sample value.
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
    float x1, x2; //!< Previous input samples
    float y1, y2; //!< Previous output samples
};

#endif // BIQUAD_FILTER_H
