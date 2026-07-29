#pragma once

#include <Arduino.h>

#ifndef BENCH_LATENCY_LOG
#define BENCH_LATENCY_LOG 0
#endif

#ifndef BENCH_EF_LATENCY_LOG
#define BENCH_EF_LATENCY_LOG 0
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

#if BENCH_EF_LATENCY_LOG
// EF samples happen in the high-tier lane; modulation is emitted later by the
// mid-tier lane. This record measures that firmware interval and the output
// enqueue attempt, not the unobservable analog-front-end settling time.
inline void benchEfLatencyHeader() {
    Serial.println(F("record,device_ms,source_us,emit_us,device_latency_us,ef_index,slot_index,ef_level,midi_value,path"));
}

inline void benchEfLatencyLog(uint8_t efIndex, uint8_t slotIndex, int efLevel,
                              uint8_t midiValue, uint32_t sourceUs,
                              const char *path) {
    const uint32_t emitUs = micros();
    Serial.print(F("ef_latency,"));
    Serial.print(millis());
    Serial.print(',');
    Serial.print(sourceUs);
    Serial.print(',');
    Serial.print(emitUs);
    Serial.print(',');
    Serial.print(static_cast<uint32_t>(emitUs - sourceUs));
    Serial.print(',');
    Serial.print(efIndex);
    Serial.print(',');
    Serial.print(slotIndex);
    Serial.print(',');
    Serial.print(efLevel);
    Serial.print(',');
    Serial.print(midiValue);
    Serial.print(',');
    Serial.println(path ? path : "midi_enqueue");
}
#else
inline void benchEfLatencyHeader() {}
inline void benchEfLatencyLog(uint8_t, uint8_t, int, uint8_t, uint32_t, const char *) {}
#endif
