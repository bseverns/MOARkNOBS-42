#pragma once

#include <Arduino.h>
#include "hal/RuntimeIO.h"

#ifndef BENCH_NOISE_LOG
#define BENCH_NOISE_LOG 0
#endif

#if BENCH_NOISE_LOG
// Print CSV header for noise log
inline void benchNoiseHeader() { Serial.println(F("ts_ms,control_id,raw_counts")); }

// Sample a pin at ~1kHz for durationMs and print CSV lines
inline void benchNoiseRun(uint8_t controlId, uint8_t pin, uint32_t durationMs) {
    uint32_t start = moar::hal::getMillis();
    uint32_t next = start;
    while (moar::hal::getMillis() - start < durationMs) {
        if (moar::hal::getMillis() >= next) {
            int raw = moar::hal::readAnalog(pin);
            Serial.print(moar::hal::getMillis());
            Serial.print(',');
            Serial.print(controlId);
            Serial.print(',');
            Serial.println(raw);
            next += 1; // ~1kHz
        }
    }
}
#else
inline void benchNoiseHeader() {}
inline void benchNoiseRun(uint8_t, uint8_t, uint32_t) {}
#endif
