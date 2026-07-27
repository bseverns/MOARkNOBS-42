#include "unity_config.h"
#include <unity.h>

#include "BootMode.h"
#include "ConfigManager.h"
#include "DiagnosticRecord.h"
#include "storage/StorageBackend.h"

#include <vector>

namespace {

class DiagnosticMemoryStorageBackend final : public StorageBackend {
  public:
    DiagnosticMemoryStorageBackend() : bytes_(16384U, 0xFFU) {}

    uint16_t length() const override { return static_cast<uint16_t>(bytes_.size()); }

    uint8_t read(int address) const override {
        if (address < 0 || static_cast<size_t>(address) >= bytes_.size()) {
            return 0xFF;
        }
        return bytes_[static_cast<size_t>(address)];
    }

    bool update(int address, uint8_t value) override {
        if (address < 0 || static_cast<size_t>(address) >= bytes_.size()) {
            return false;
        }
        bytes_[static_cast<size_t>(address)] = value;
        return true;
    }

    void readBytes(int address, void *dest, size_t len) const override {
        auto *out = static_cast<uint8_t *>(dest);
        for (size_t idx = 0; idx < len; ++idx) {
            out[idx] = read(address + static_cast<int>(idx));
        }
    }

    bool writeBytes(int address, const void *src, size_t len) override {
        const auto *in = static_cast<const uint8_t *>(src);
        bool written = true;
        for (size_t idx = 0; idx < len; ++idx) {
            written = update(address + static_cast<int>(idx), in[idx]) && written;
        }
        return written;
    }

  private:
    std::vector<uint8_t> bytes_;
};

} // namespace

void test_diagnostic_record_persists_boot_and_apply_events() {
    DiagnosticMemoryStorageBackend storage;
    ConfigManager::setStorageBackend(&storage);
    DiagnosticRecord::testOnly_resetState();

    DiagnosticRecord::initialize();
    DiagnosticRecord::recordResetSnapshot(0x00000040UL, 3);
    DiagnosticRecord::recordBootMode(static_cast<uint8_t>(BootMode::UsbConfigurator));
    DiagnosticRecord::recordConfigLoadSource(
        static_cast<uint8_t>(ConfigManager::LoadSource::kBackup));
    DiagnosticRecord::recordConfigApplyResult(DiagnosticRecord::ConfigApplyStatus::Acked, "abc123");
    DiagnosticRecord::recordProtocolError("checksum");
    DiagnosticRecord::recordDisplayInitFailures(2);
    DiagnosticRecord::recordLoopOverrunHighWater(4096);

    DiagnosticRecord::testOnly_resetState();
    const DiagnosticRecord::PersistentRecord &record = DiagnosticRecord::snapshot();
    TEST_ASSERT_EQUAL_HEX32(0x00000040UL, record.lastResetReason);
    TEST_ASSERT_EQUAL_UINT16(3, record.brownoutCount);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(BootMode::UsbConfigurator), record.lastBootMode);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConfigManager::LoadSource::kBackup),
                            record.lastConfigLoadSource);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DiagnosticRecord::ConfigApplyStatus::Acked),
                            record.lastConfigApplyStatus);
    TEST_ASSERT_EQUAL_STRING("abc123", record.lastConfigApplyChecksum);
    TEST_ASSERT_EQUAL_STRING("checksum", record.lastProtocolErrorCode);
    TEST_ASSERT_EQUAL_UINT16(2, record.displayInitFailures);
    TEST_ASSERT_EQUAL_UINT32(4096, record.maxLoopOverrunMicros);

    ConfigManager::setStorageBackend(nullptr);
    DiagnosticRecord::testOnly_resetState();
}

void test_diagnostic_record_rejects_corrupt_storage() {
    DiagnosticMemoryStorageBackend storage;
    ConfigManager::setStorageBackend(&storage);
    DiagnosticRecord::testOnly_resetState();

    DiagnosticRecord::PersistentRecord corrupt{};
    corrupt.magic = 0xDEADBEEFU;
    DiagnosticRecord::testOnly_forcePersistedRecord(corrupt);
    DiagnosticRecord::testOnly_resetState();

    const DiagnosticRecord::PersistentRecord &record = DiagnosticRecord::snapshot();
    TEST_ASSERT_EQUAL_UINT32(0, record.lastResetReason);
    TEST_ASSERT_EQUAL_UINT16(0, record.brownoutCount);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DiagnosticRecord::ConfigApplyStatus::None),
                            record.lastConfigApplyStatus);
    TEST_ASSERT_EQUAL_STRING("", record.lastConfigApplyChecksum);
    TEST_ASSERT_EQUAL_STRING("", record.lastProtocolErrorCode);

    ConfigManager::setStorageBackend(nullptr);
    DiagnosticRecord::testOnly_resetState();
}

void test_diagnostic_record_loop_overrun_only_grows() {
    DiagnosticMemoryStorageBackend storage;
    ConfigManager::setStorageBackend(&storage);
    DiagnosticRecord::testOnly_resetState();

    DiagnosticRecord::initialize();
    DiagnosticRecord::recordLoopOverrunHighWater(2800);
    DiagnosticRecord::recordLoopOverrunHighWater(1200);
    DiagnosticRecord::recordLoopOverrunHighWater(3100);

    const DiagnosticRecord::PersistentRecord &record = DiagnosticRecord::snapshot();
    TEST_ASSERT_EQUAL_UINT32(3100, record.maxLoopOverrunMicros);

    ConfigManager::setStorageBackend(nullptr);
    DiagnosticRecord::testOnly_resetState();
}
