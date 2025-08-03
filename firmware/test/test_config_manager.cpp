#include <unity.h>
#include <EEPROM.h>
#include "ConfigManager.h"
#include "TestHelpers.h"

// Fake the EEPROM header so the primary copy looks rotten
// while the backup stays pristine. The ConfigManager should
// sniff this out and pull the backup instead.

void corrupt_primary_valid_backup() {
    // wipe primary magic
    EEPROM.write(EEPROM_MAGIC_ADDRESS, 0x00);
    EEPROM.write(EEPROM_MAGIC_ADDRESS + 1, 0x00);
    // write backup magic
    EEPROM.write(EEPROM_MAGIC_ADDRESS + 2, (EEPROM_MAGIC_BACKUP >> 8) & 0xFF);
    EEPROM.write(EEPROM_MAGIC_ADDRESS + 3, EEPROM_MAGIC_BACKUP & 0xFF);

    // stash some known data in the backup region
    const int backupOffset = EEPROM_BACKUP_START + EEPROM_POT_CHANNELS;
    EEPROM.write(backupOffset, 5);
    EEPROM.write(backupOffset + 1, 7);

    ConfigManager cfg(2, 0);
    std::vector<uint8_t> pots;
    bool ok = cfg.loadConfiguration(pots);

    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_UINT8(5, pots[0]);
    TEST_ASSERT_EQUAL_UINT8(7, pots[1]);
}

// Trash both headers and the manager should bail to defaults
void corrupted_primary_and_backup() {
    EEPROM.write(EEPROM_MAGIC_ADDRESS, 0x00);
    EEPROM.write(EEPROM_MAGIC_ADDRESS + 1, 0x00);
    EEPROM.write(EEPROM_MAGIC_ADDRESS + 2, 0x00);
    EEPROM.write(EEPROM_MAGIC_ADDRESS + 3, 0x00);

    ConfigManager cfg(2, 0);
    std::vector<uint8_t> pots;
    bool ok = cfg.loadConfiguration(pots);

    TEST_ASSERT_FALSE(ok);
    TEST_ASSERT_EQUAL_UINT8(1, pots[0]);
    TEST_ASSERT_EQUAL_UINT8(1, pots[1]);
}

void setUp(void) {}
void tearDown(void) {}

void setup() {
    UNITY_BEGIN();
    RUN_TEST(corrupt_primary_valid_backup);
    RUN_TEST(corrupted_primary_and_backup);
    UNITY_END();
}

void loop() {}
