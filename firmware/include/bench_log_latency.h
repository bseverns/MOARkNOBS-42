#pragma once

#include <Arduino.h>

#ifndef BENCH_LATENCY_LOG
#define BENCH_LATENCY_LOG 0
#endif

#if BENCH_LATENCY_LOG
// Print CSV header for latency log
inline void benchLatencyHeader() { Serial.println(F("ts_ms,control_id,delta_ms,path,notes")); }

// Log a latency sample in CSV format
inline void benchLatencyLog(uint8_t controlId, uint32_t tScanUs, const char *path,
                            const char *notes = "") {
    float delta_ms = (micros() - tScanUs) / 1000.0f;
    Serial.print(millis());
    Serial.print(',');
    Serial.print(controlId);
    Serial.print(',');
    Serial.print(delta_ms, 3);
    Serial.print(',');
    Serial.print(path);
    Serial.print(',');
    Serial.println(notes);
}
#else
inline void benchLatencyHeader() {}
inline void benchLatencyLog(uint8_t, uint32_t, const char *, const char *) {}
#endif
