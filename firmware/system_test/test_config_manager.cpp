#include <Arduino.h>
#include "SystemTestShim.h"
#include "ConfigManager.h"
#include "TestHelpers.h"

// ConfigManager guards persistent storage like a hawk. These integration tests flip the
// primary/backup magic bytes so we can confirm the recovery logic keeps user
// presets alive when corruption strikes.

// Extra tests live inside ConfigManager.cpp under UNIT_TEST
extern void test_eeprom_recovery_after_power_cycle();
extern void test_calibration_offsets_survive_power_cycle();

namespace {
StorageBackend *activeStorage() { return ConfigManager::getStorageBackend(); }

void writeStorageByte(int address, uint8_t value) { activeStorage()->update(address, value); }
} // namespace

// Fake the storage header so the primary copy looks rotten
// while the backup stays pristine. The ConfigManager should
// sniff this out and pull the backup instead.

// If the primary header gets trashed but the backup is intact we should fall
// back gracefully and restore the saved channels.
void corrupt_primary_valid_backup() {
    // wipe primary magic
    writeStorageByte(EEPROM_MAGIC_ADDRESS, 0x00);
    writeStorageByte(EEPROM_MAGIC_ADDRESS + 1, 0x00);
    // write backup magic
    writeStorageByte(EEPROM_MAGIC_ADDRESS + 2, (EEPROM_MAGIC_BACKUP >> 8) & 0xFF);
    writeStorageByte(EEPROM_MAGIC_ADDRESS + 3, EEPROM_MAGIC_BACKUP & 0xFF);

    // stash some known data in the backup region
    const int backupOffset = EEPROM_BACKUP_START + EEPROM_POT_CHANNELS;
    writeStorageByte(backupOffset, 5);
    writeStorageByte(backupOffset + 1, 7);

    ConfigManager cfg(2, 0);
    std::vector<uint8_t> pots;
    bool ok = cfg.loadConfiguration(pots);

    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_UINT8(5, pots[0]);
    TEST_ASSERT_EQUAL_UINT8(7, pots[1]);
}

// Trash both headers and the manager should bail to defaults
// When both headers are rotten we expect the manager to bail to defaults and
// report the failure so the UI can warn the user.
void corrupted_primary_and_backup() {
    writeStorageByte(EEPROM_MAGIC_ADDRESS, 0x00);
    writeStorageByte(EEPROM_MAGIC_ADDRESS + 1, 0x00);
    writeStorageByte(EEPROM_MAGIC_ADDRESS + 2, 0x00);
    writeStorageByte(EEPROM_MAGIC_ADDRESS + 3, 0x00);

    ConfigManager cfg(2, 0);
    std::vector<uint8_t> pots;
    bool ok = cfg.loadConfiguration(pots);

    TEST_ASSERT_FALSE(ok);
    TEST_ASSERT_EQUAL_UINT8(1, pots[0]);
    TEST_ASSERT_EQUAL_UINT8(1, pots[1]);
}
