#include <Arduino.h>
#include <EEPROM.h>
#include <unity.h>
#include "ConfigManager.h"
#include "TestHelpers.h"

// Extra tests live inside ConfigManager.cpp under UNIT_TEST
extern void test_eeprom_recovery_after_power_cycle();
extern void test_calibration_offsets_survive_power_cycle();
void test_high_index_envelope_assignment_survives_reload();

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

void test_high_index_envelope_assignment_survives_reload() {
    auto pm = createPotentiometerManager();
    auto envelopes = createEnvelopeFollowers(&pm);
    ConfigManager cfg(NUM_POTS, NUM_BUTTONS);

    std::map<int, int> assignments;
    for (int pot = 0; pot < NUM_POTS; ++pot) {
        assignments[pot] = ENVELOPE_UNASSIGNED;
    }

    const int highPot = NUM_POTS - 1;
    const int envIndex = NUM_ENVELOPES - 1;
    const float baseline = 0.37f;
    assignments[highPot] = envIndex;
    envelopes[envIndex].setBaseline(baseline);

    cfg.saveEnvelopeSettings(assignments, envelopes);

    std::map<int, int> restored;
    auto freshEnvelopes = createEnvelopeFollowers(&pm);
    bool baselinesOk = cfg.loadEnvelopeSettings(restored, freshEnvelopes);

    TEST_ASSERT_TRUE(baselinesOk);
    TEST_ASSERT_EQUAL_INT(NUM_POTS, restored.size());
    TEST_ASSERT_EQUAL(envIndex, restored[highPot]);
    TEST_ASSERT_EQUAL(ENVELOPE_UNASSIGNED, restored[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, baseline, freshEnvelopes[envIndex].getBaseline());
}
