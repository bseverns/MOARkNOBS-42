#ifndef MN42_DIAGNOSTIC_RECORD_H
#define MN42_DIAGNOSTIC_RECORD_H

#include <ArduinoJson.h>
#include <cstdint>

namespace DiagnosticRecord {

enum class ConfigApplyStatus : uint8_t {
    None = 0,
    Acked = 1,
    Error = 2,
};

struct PersistentRecord {
    uint32_t magic = 0;
    uint16_t version = 0;
    uint16_t size = 0;
    uint32_t lastResetReason = 0;
    uint16_t brownoutCount = 0;
    uint16_t displayInitFailures = 0;
    uint8_t lastBootMode = 0;
    uint8_t lastConfigLoadSource = 0;
    uint8_t lastConfigApplyStatus = static_cast<uint8_t>(ConfigApplyStatus::None);
    uint8_t watchdogResetMarker = 0;
    uint8_t watchdogResetMarkerKnown = 0;
    uint8_t reserved[3] = {0, 0, 0};
    uint32_t maxLoopOverrunMicros = 0;
    char lastConfigApplyChecksum[40] = {0};
    char lastProtocolErrorCode[24] = {0};
    uint16_t crc = 0;
};

void initialize();
void recordResetSnapshot(uint32_t resetReason, uint16_t brownoutCount);
void recordBootMode(uint8_t bootMode);
void recordConfigLoadSource(uint8_t loadSource);
void recordConfigApplyResult(ConfigApplyStatus status, const char *checksum);
void recordProtocolError(const char *code);
void recordDisplayInitFailures(uint32_t failures);
void recordLoopOverrunHighWater(uint32_t durationMicros);

const PersistentRecord &snapshot();
bool persistenceEnabled();
void writeJson(JsonObject object);

#if defined(UNIT_TEST)
void testOnly_resetState();
void testOnly_forcePersistedRecord(const PersistentRecord &record);
#endif

} // namespace DiagnosticRecord

#endif // MN42_DIAGNOSTIC_RECORD_H
