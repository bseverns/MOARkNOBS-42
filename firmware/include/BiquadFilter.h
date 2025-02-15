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
         * @param type: The filter type (LOWPASS, HIGHPASS, BANDPASS).
         * @param frequency: The cutoff or center frequency in Hz.
         * @param sampleRate: The sampling rate of the system in Hz.
         * @param q: The "quality factor" controlling the sharpness of the filter's frequency response.
         */
        void configure(FilterType type, float frequency, float sampleRate, float q = 0.707) {
            // omega represents the normalized angular frequency of the cutoff/center frequency
            // in radians per sample.
            float omega = 2.0 * PI * frequency / sampleRate;
    
            // cos_omega and sin_omega are used to compute coefficients related to the phase and gain
            // behavior of the filter.
            float cos_omega = cos(omega);
            float sin_omega = sin(omega);
    
            // alpha controls the bandwidth of the filter. For low-pass and high-pass filters,
            // it determines the slope of the roll-off. For band-pass filters, it defines the width
            // of the passband. Q affects alpha directly.
            float alpha = sin_omega / (2.0 * q);
    
            switch (type) {
                case LOWPASS:
                    // Low-pass filter coefficients (normalized)
                    // These equations come from the standard biquad filter design equations
                    a0 = (1 - cos_omega) / 2.0;  // Scales the input for low frequencies
                    a1 = 1 - cos_omega;          // Ensures symmetry for smoothing
                    a2 = (1 - cos_omega) / 2.0;  // Symmetry in forward gain
                    b1 = -2.0 * cos_omega;       // Feedback to suppress high frequencies
                    b2 = 1.0 - alpha;            // Feedback gain for stability
                    break;
    
                case HIGHPASS:
                    // High-pass filter coefficients (normalized)
                    a0 = (1 + cos_omega) / 2.0;  // Scales the input for high frequencies
                    a1 = -(1 + cos_omega);       // Inverts signal at low frequencies
                    a2 = (1 + cos_omega) / 2.0;  // Symmetry in forward gain
                    b1 = -2.0 * cos_omega;       // Feedback to suppress low frequencies
                    b2 = 1.0 - alpha;            // Feedback gain for stability
                    break;
    
                case BANDPASS:
                    // Band-pass filter coefficients (normalized)
                    // Center frequency is emphasized, while others are attenuated
                    a0 = alpha;         // Proportional to the width of the passband
                    a1 = 0.0;           // No additional scaling for symmetry
                    a2 = -alpha;        // Inverts out-of-band frequencies
                    b1 = -2.0 * cos_omega; // Feedback to control the center frequency
                    b2 = 1.0 - alpha;   // Feedback gain for stability
                    break;
            }
    
            // Normalize coefficients so the gain at the cutoff frequency is consistent
            float norm = 1.0 + alpha;
            a0 /= norm;
            a1 /= norm;
            a2 /= norm;
            b1 /= norm;
            b2 /= norm;
        }
    
        /**
         * Process a single audio sample through the filter.
         * @param input: The raw audio sample.
         * @return The filtered audio sample.
         */
        float process(float input) {
            // The output is computed based on the biquad difference equation:
            // y[n] = a0*x[n] + a1*x[n-1] + a2*x[n-2] - b1*y[n-1] - b2*y[n-2]
            float output = a0 * input + a1 * z1 + a2 * z2 - b1 * z1 - b2 * z2;
    
            // Update state variables for the next input
            z2 = z1;   // Save the previous state
            z1 = output;
    
            return output;
        }
    
    private:
        // Filter coefficients for the difference equation
        float a0, a1, a2; // Forward (feed-forward) coefficients
        float b1, b2;     // Feedback (feed-back) coefficients
    
        // State variables for the filter's memory
        float z1, z2;     // Delayed samples for recursion
    };
    
    #endif //FILTER