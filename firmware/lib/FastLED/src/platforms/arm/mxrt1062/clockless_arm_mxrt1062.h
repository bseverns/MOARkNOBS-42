#ifndef __INC_CLOCKLESS_ARM_MXRT1062_H
#define __INC_CLOCKLESS_ARM_MXRT1062_H

#include <Arduino.h>
#include <FastLED.h>

FASTLED_NAMESPACE_BEGIN

// Simplified placeholder for the FastLED MXRT1062 clockless driver.
// This file is normally provided by the FastLED library. It has been
// copied into the repo so it can be patched locally.

template <int DATA_PIN, int T1, int T2, int T3>
class Clockless_arm_mxrt1062 {
public:
    static void init() {
        FastPin<DATA_PIN>::setOutput();
        FastPin<DATA_PIN>::lo();
    }

    template <typename RGB_ORDER>
    static void showPixels(PixelController<RGB_ORDER> &pixels) {
        // Actual implementation omitted in this placeholder.
        uint32_t wait_off [[maybe_unused]] = 0;
        (void)wait_off;
    }
};

FASTLED_NAMESPACE_END

#endif // __INC_CLOCKLESS_ARM_MXRT1062_H
