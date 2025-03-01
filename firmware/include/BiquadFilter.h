#ifndef BIQUAD_FILTER_H
#define BIQUAD_FILTER_H

class BiquadFilter {
public:
    enum FilterType {
        LOWPASS,   // Allows frequencies below a cutoff frequency to pass
        HIGHPASS,  // Allows frequencies above a cutoff frequency to pass
        BANDPASS   // Allows frequencies around a center frequency to pass
    };

    BiquadFilter() : a0(0), a1(0), a2(0), b1(0), b2(0), z1(0), z2(0) {}

    /**
     * Configure the filter coefficients based on the desired type, frequency, Q factor, and sampling rate.
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

    float process(float input) {
        float output = a0 * input + a1 * z1 + a2 * z2 - b1 * z1 - b2 * z2;
        z2 = z1;
        z1 = output;
        return output;
    }

private:
    float a0, a1, a2;
    float b1, b2;
    float z1, z2;
};

#endif // BIQUAD_FILTER_H
