#pragma once

#include <Arduino.h>

#ifndef BENCH_NOISE_LOG
#define BENCH_NOISE_LOG 0
#endif

#if BENCH_NOISE_LOG
// Print CSV header for noise log
inline void benchNoiseHeader() { Serial.println(F("ts_ms,control_id,raw_counts")); }

// Sample a pin at ~1kHz for durationMs and print CSV lines
inline void benchNoiseRun(uint8_t controlId, uint8_t pin, uint32_t durationMs) {
    uint32_t start = millis();
    uint32_t next = start;
    while (millis() - start < durationMs) {
        if (millis() >= next) {
            int raw = analogRead(pin);
            Serial.print(millis());
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
