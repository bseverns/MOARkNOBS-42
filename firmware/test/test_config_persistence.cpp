#include "unity_config.h"
#include <unity.h>

#include "ConfigManager.h"
#include "storage/StorageBackend.h"

#include <vector>

namespace {

class MemoryStorageBackend final : public StorageBackend {
  public:
    MemoryStorageBackend()
        : bytes_(EEPROM_PROFILE_SETTINGS_START(NUM_PROFILES) + EEPROM_PROFILE_SETTINGS_BLOCK_SIZE +
                     64U,
                 0x00) {}

    uint16_t length() const override { return static_cast<uint16_t>(bytes_.size()); }

    uint8_t read(int address) const override {
        if (address < 0 || static_cast<size_t>(address) >= bytes_.size()) {
            return 0;
        }
        return bytes_[static_cast<size_t>(address)];
    }

    void update(int address, uint8_t value) override {
        if (address < 0 || static_cast<size_t>(address) >= bytes_.size()) {
            return;
        }
        if (dropPrimaryWrites_ && isBlockedPrimaryAddress(address)) {
            return;
        }
        bytes_[static_cast<size_t>(address)] = value;
    }

    void readBytes(int address, void *dest, size_t len) const override {
        auto *out = static_cast<uint8_t *>(dest);
        for (size_t i = 0; i < len; ++i) {
            out[i] = read(address + static_cast<int>(i));
        }
    }

    void writeBytes(int address, const void *src, size_t len) override {
        const auto *in = static_cast<const uint8_t *>(src);
        for (size_t i = 0; i < len; ++i) {
            update(address + static_cast<int>(i), in[i]);
        }
    }

    void fill(uint8_t value) { std::fill(bytes_.begin(), bytes_.end(), value); }

    void clearPrimaryMagic(uint16_t base) {
        update(base + EEPROM_MAGIC_ADDRESS, 0x00);
        update(base + EEPROM_MAGIC_ADDRESS + 1, 0x00);
    }

    void clearBackupMagic(uint16_t base) {
        update(base + EEPROM_MAGIC_ADDRESS + 2, 0x00);
        update(base + EEPROM_MAGIC_ADDRESS + 3, 0x00);
    }

    void copyPrimaryConfigToBackup(uint16_t base) {
        for (uint16_t offset = EEPROM_START_ADDRESS; offset < EEPROM_BACKUP_START; ++offset) {
            update(base + EEPROM_BACKUP_START + offset, read(base + offset));
        }
        writeMagic(base + EEPROM_MAGIC_ADDRESS + 2, EEPROM_MAGIC_BACKUP);
    }

    void setDropPrimaryWrites(uint16_t base, bool enabled) {
        blockedPrimaryBase_ = base;
        dropPrimaryWrites_ = enabled;
    }

  private:
    bool isBlockedPrimaryAddress(int address) const {
        const int base = static_cast<int>(blockedPrimaryBase_);
        if (address >= base + EEPROM_START_ADDRESS && address < base + EEPROM_BACKUP_START) {
            return true;
        }
        return address == base + EEPROM_MAGIC_ADDRESS || address == base + EEPROM_MAGIC_ADDRESS + 1;
    }

    void writeMagic(int address, uint16_t magic) {
        update(address, static_cast<uint8_t>((magic >> 8) & 0xFF));
        update(address + 1, static_cast<uint8_t>(magic & 0xFF));
    }

    std::vector<uint8_t> bytes_;
    uint16_t blockedPrimaryBase_ = EEPROM_PROFILE_START(0);
    bool dropPrimaryWrites_ = false;
};

void configureStoredValues(ConfigManager &cfg, uint8_t channel, uint8_t cc) {
    cfg.setPotChannel(0, channel);
    cfg.setPotCCNumber(0, cc);
}

} // namespace

void test_config_load_prefers_primary_when_backup_copy_is_invalid() {
    MemoryStorageBackend storage;
    ConfigManager::setStorageBackend(&storage);

    ConfigManager cfg(NUM_POTS, NUM_BUTTONS);
    configureStoredValues(cfg, 5, 77);
    cfg.saveConfiguration();
    storage.clearBackupMagic(EEPROM_PROFILE_START(0));

    ConfigManager rebooted(NUM_POTS, NUM_BUTTONS);
    std::vector<uint8_t> pots;
    TEST_ASSERT_TRUE(rebooted.loadConfiguration(pots));
    TEST_ASSERT_EQUAL_UINT8(5, pots[0]);
    TEST_ASSERT_EQUAL_UINT8(77, rebooted.getPotCCNumber(0));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConfigManager::LoadSource::kPrimary),
                            static_cast<uint8_t>(rebooted.getLastLoadSource()));

    ConfigManager::setStorageBackend(nullptr);
}

void test_config_load_restores_from_backup_and_repairs_primary() {
    MemoryStorageBackend storage;
    ConfigManager::setStorageBackend(&storage);

    ConfigManager cfg(NUM_POTS, NUM_BUTTONS);
    configureStoredValues(cfg, 9, 88);
    cfg.saveConfiguration();
    storage.copyPrimaryConfigToBackup(EEPROM_PROFILE_START(0));
    storage.clearPrimaryMagic(EEPROM_PROFILE_START(0));

    ConfigManager rebooted(NUM_POTS, NUM_BUTTONS);
    std::vector<uint8_t> pots;
    TEST_ASSERT_TRUE(rebooted.loadConfiguration(pots));
    TEST_ASSERT_EQUAL_UINT8(9, pots[0]);
    TEST_ASSERT_EQUAL_UINT8(88, rebooted.getPotCCNumber(0));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConfigManager::LoadSource::kBackup),
                            static_cast<uint8_t>(rebooted.getLastLoadSource()));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConfigManager::RecoveryEvent::kBackupRestored),
                            static_cast<uint8_t>(rebooted.consumeRecoveryEvent()));
    TEST_ASSERT_TRUE(rebooted.hasHealthyConfigurationCopy(false));

    ConfigManager::setStorageBackend(nullptr);
}

void test_config_load_resets_to_defaults_when_primary_and_backup_are_both_corrupt() {
    MemoryStorageBackend storage;
    storage.fill(0x00);
    ConfigManager::setStorageBackend(&storage);

    ConfigManager cfg(NUM_POTS, NUM_BUTTONS);
    std::vector<uint8_t> pots;
    TEST_ASSERT_FALSE(cfg.loadConfiguration(pots));
    TEST_ASSERT_EQUAL_UINT8(1, cfg.getPotChannel(0));
    TEST_ASSERT_EQUAL_UINT8(0, cfg.getPotCCNumber(0));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConfigManager::LoadSource::kDefaults),
                            static_cast<uint8_t>(cfg.getLastLoadSource()));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConfigManager::RecoveryEvent::kDefaultsLoaded),
                            static_cast<uint8_t>(cfg.consumeRecoveryEvent()));
    TEST_ASSERT_TRUE(cfg.hasHealthyConfigurationCopy(false));

    ConfigManager::setStorageBackend(nullptr);
}

void test_profile_save_interruption_leaves_latest_copy_in_backup() {
    MemoryStorageBackend storage;
    ConfigManager::setStorageBackend(&storage);

    const uint8_t profileId = 2;
    const uint16_t base = EEPROM_PROFILE_START(profileId);

    ConfigManager cfg(NUM_POTS, NUM_BUTTONS);
    configureStoredValues(cfg, 11, 93);
    storage.setDropPrimaryWrites(base, true);
    cfg.saveProfile(profileId);
    storage.setDropPrimaryWrites(base, false);

    ConfigManager rebooted(NUM_POTS, NUM_BUTTONS);
    std::vector<uint8_t> pots;
    TEST_ASSERT_TRUE(rebooted.loadConfiguration(pots, base));
    TEST_ASSERT_EQUAL_UINT8(11, pots[0]);
    TEST_ASSERT_EQUAL_UINT8(93, rebooted.getPotCCNumber(0));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConfigManager::LoadSource::kBackup),
                            static_cast<uint8_t>(rebooted.getLastLoadSource()));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConfigManager::RecoveryEvent::kBackupRestored),
                            static_cast<uint8_t>(rebooted.consumeRecoveryEvent()));
    TEST_ASSERT_TRUE(rebooted.hasHealthyConfigurationCopy(false, base));

    ConfigManager::setStorageBackend(nullptr);
}
