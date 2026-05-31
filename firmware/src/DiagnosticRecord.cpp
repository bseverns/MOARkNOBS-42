#include "DiagnosticRecord.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "ConfigManager.h"
#include "Globals.h"
#include "protocol/SceneStorage.h"

namespace DiagnosticRecord {
namespace {

constexpr uint32_t kMagic = 0x4D4E3432; // "MN42"
constexpr uint16_t kVersion = 1;

struct MacroRecordLayout {
    uint16_t version = 1;
    uint16_t crc = 0;
    uint8_t occupied = 0;
    SceneStorage::ConfigState state{};
};

struct SceneRecordLayout {
    uint16_t version = 1;
    uint16_t crc = 0;
    uint8_t occupied = 0;
    char name[16] = {0};
    SceneStorage::ConfigState state{};
};

constexpr uint16_t kMacroStorageAddress =
    EEPROM_PROFILE_SETTINGS_BASE + NUM_PROFILES * EEPROM_PROFILE_SETTINGS_BLOCK_SIZE;
constexpr uint16_t kSceneStorageBase =
    static_cast<uint16_t>(kMacroStorageAddress + sizeof(MacroRecordLayout));
constexpr uint16_t kSceneStorageEnd = static_cast<uint16_t>(
    kSceneStorageBase + SceneStorage::kSceneSlotCount * sizeof(SceneRecordLayout));
constexpr uint16_t kRecordStorageAddress = kSceneStorageEnd;

PersistentRecord g_record{};
bool g_loaded = false;
bool g_persistenceEnabled = false;

StorageBackend &activeStorageBackend() { return *ConfigManager::getStorageBackend(); }

template <typename T> void storageGet(int address, T &value) {
    activeStorageBackend().readBytes(address, &value, sizeof(T));
}

template <typename T> void storagePut(int address, const T &value) {
    activeStorageBackend().writeBytes(address, &value, sizeof(T));
}

uint16_t crc16Update(uint16_t crc, uint8_t data) {
    crc ^= static_cast<uint16_t>(data) << 8;
    for (uint8_t bit = 0; bit < 8; ++bit) {
        if (crc & 0x8000U) {
            crc = static_cast<uint16_t>((crc << 1U) ^ 0x1021U);
        } else {
            crc = static_cast<uint16_t>(crc << 1U);
        }
    }
    return crc;
}

uint16_t computeRecordCrc(const PersistentRecord &record) {
    const uint8_t *bytes = reinterpret_cast<const uint8_t *>(&record);
    const size_t crcOffset = offsetof(PersistentRecord, crc);
    uint16_t crc = 0xFFFFU;
    for (size_t idx = 0; idx < crcOffset; ++idx) {
        crc = crc16Update(crc, bytes[idx]);
    }
    return crc;
}

bool storageHasRoom() {
    const uint32_t required =
        static_cast<uint32_t>(kRecordStorageAddress) + sizeof(PersistentRecord);
    return activeStorageBackend().length() >= required;
}

void resetRecordDefaults(PersistentRecord &record) {
    record = {};
    record.magic = kMagic;
    record.version = kVersion;
    record.size = sizeof(PersistentRecord);
}

void sanitizeRecord(PersistentRecord &record) {
    if (record.magic != kMagic || record.version != kVersion ||
        record.size != sizeof(PersistentRecord) || record.crc != computeRecordCrc(record)) {
        resetRecordDefaults(record);
        return;
    }

    record.lastConfigApplyChecksum[sizeof(record.lastConfigApplyChecksum) - 1] = '\0';
    record.lastProtocolErrorCode[sizeof(record.lastProtocolErrorCode) - 1] = '\0';

    if (record.lastConfigApplyStatus > static_cast<uint8_t>(ConfigApplyStatus::Error)) {
        record.lastConfigApplyStatus = static_cast<uint8_t>(ConfigApplyStatus::None);
    }
    record.watchdogResetMarker = record.watchdogResetMarker ? 1 : 0;
    record.watchdogResetMarkerKnown = record.watchdogResetMarkerKnown ? 1 : 0;
}

void persistRecordIfChanged(const PersistentRecord &before) {
    if (std::memcmp(&before, &g_record, sizeof(g_record)) == 0) {
        return;
    }
    if (!g_persistenceEnabled) {
        return;
    }
    g_record.crc = computeRecordCrc(g_record);
    storagePut(kRecordStorageAddress, g_record);
}

void ensureLoaded() {
    if (g_loaded) {
        return;
    }

    resetRecordDefaults(g_record);
    g_persistenceEnabled = storageHasRoom();
    if (g_persistenceEnabled) {
        PersistentRecord stored{};
        storageGet(kRecordStorageAddress, stored);
        sanitizeRecord(stored);
        g_record = stored;
        g_record.crc = computeRecordCrc(g_record);
        storagePut(kRecordStorageAddress, g_record);
    }
    g_loaded = true;
}

void copyText(char *dest, size_t capacity, const char *src) {
    if (!dest || capacity == 0) {
        return;
    }
    std::memset(dest, 0, capacity);
    if (!src || src[0] == '\0') {
        return;
    }
    std::snprintf(dest, capacity, "%s", src);
}

const char *describeBootMode(uint8_t bootMode) {
    switch (bootMode) {
    case 0:
        return "standalone_runtime";
    case 1:
        return "usb_configurator";
    default:
        return "unknown";
    }
}

const char *describeLoadSource(uint8_t source) {
    switch (source) {
    case 1:
        return "primary";
    case 2:
        return "backup";
    case 3:
        return "defaults";
    default:
        return "unknown";
    }
}

const char *describeConfigApplyStatus(uint8_t status) {
    switch (static_cast<ConfigApplyStatus>(status)) {
    case ConfigApplyStatus::Acked:
        return "acked";
    case ConfigApplyStatus::Error:
        return "error";
    default:
        return "none";
    }
}

void detectWatchdogResetMarker(uint32_t /*resetReason*/, uint8_t &known, uint8_t &marker) {
    known = 0;
    marker = 0;
}

} // namespace

void initialize() { ensureLoaded(); }

void recordResetSnapshot(uint32_t resetReason, uint16_t brownoutCount) {
    ensureLoaded();
    PersistentRecord before = g_record;
    g_record.lastResetReason = resetReason;
    g_record.brownoutCount = brownoutCount;
    detectWatchdogResetMarker(resetReason, g_record.watchdogResetMarkerKnown,
                              g_record.watchdogResetMarker);
    persistRecordIfChanged(before);
}

void recordBootMode(uint8_t bootMode) {
    ensureLoaded();
    PersistentRecord before = g_record;
    g_record.lastBootMode = bootMode;
    persistRecordIfChanged(before);
}

void recordConfigLoadSource(uint8_t loadSource) {
    ensureLoaded();
    PersistentRecord before = g_record;
    g_record.lastConfigLoadSource = loadSource;
    persistRecordIfChanged(before);
}

void recordConfigApplyResult(ConfigApplyStatus status, const char *checksum) {
    ensureLoaded();
    PersistentRecord before = g_record;
    g_record.lastConfigApplyStatus = static_cast<uint8_t>(status);
    copyText(g_record.lastConfigApplyChecksum, sizeof(g_record.lastConfigApplyChecksum), checksum);
    persistRecordIfChanged(before);
}

void recordProtocolError(const char *code) {
    ensureLoaded();
    PersistentRecord before = g_record;
    copyText(g_record.lastProtocolErrorCode, sizeof(g_record.lastProtocolErrorCode), code);
    persistRecordIfChanged(before);
}

void recordDisplayInitFailures(uint32_t failures) {
    ensureLoaded();
    PersistentRecord before = g_record;
    g_record.displayInitFailures =
        static_cast<uint16_t>(std::min<uint32_t>(failures, static_cast<uint32_t>(UINT16_MAX)));
    persistRecordIfChanged(before);
}

void recordLoopOverrunHighWater(uint32_t durationMicros) {
    ensureLoaded();
    if (durationMicros <= g_record.maxLoopOverrunMicros) {
        return;
    }
    PersistentRecord before = g_record;
    g_record.maxLoopOverrunMicros = durationMicros;
    persistRecordIfChanged(before);
}

const PersistentRecord &snapshot() {
    ensureLoaded();
    return g_record;
}

bool persistenceEnabled() {
    ensureLoaded();
    return g_persistenceEnabled;
}

void writeJson(JsonObject object) {
    const PersistentRecord &record = snapshot();
    char resetHex[11] = {0};
    std::snprintf(resetHex, sizeof(resetHex), "0x%08lX",
                  static_cast<unsigned long>(record.lastResetReason));

    object["type"] = "diagnostics";
    object["storage_mode"] = persistenceEnabled() ? "persistent" : "volatile";
    object["last_reset_reason_raw"] = record.lastResetReason;
    object["last_reset_reason_hex"] = resetHex;
    object["last_boot_mode"] = describeBootMode(record.lastBootMode);
    object["last_config_load_source"] = describeLoadSource(record.lastConfigLoadSource);
    object["last_config_apply_status"] = describeConfigApplyStatus(record.lastConfigApplyStatus);
    object["last_config_apply_checksum"] = record.lastConfigApplyChecksum;
    object["last_protocol_error_code"] = record.lastProtocolErrorCode;
    object["brownout_count"] = record.brownoutCount;
    object["display_init_failures"] = record.displayInitFailures;
    object["max_loop_overrun_us"] = record.maxLoopOverrunMicros;
    object["watchdog_reset_marker_available"] = record.watchdogResetMarkerKnown != 0;
    if (record.watchdogResetMarkerKnown != 0) {
        object["watchdog_reset_marker"] = record.watchdogResetMarker != 0;
    }
}

#if defined(UNIT_TEST)
void testOnly_resetState() {
    g_record = {};
    g_loaded = false;
    g_persistenceEnabled = false;
}

void testOnly_forcePersistedRecord(const PersistentRecord &record) {
    testOnly_resetState();
    if (!storageHasRoom()) {
        return;
    }
    storagePut(kRecordStorageAddress, record);
}
#endif

} // namespace DiagnosticRecord
