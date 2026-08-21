#include <Arduino.h>
#include "SystemTestShim.h"
#include "ConfigManager.h"
#include "TestHelpers.h"

// ConfigManager guards persistent storage like a hawk. These integration tests flip the
// primary/backup magic bytes so we can confirm the recovery logic keeps user
// presets alive when corruption strikes.

namespace {
StorageBackend *activeStorage() { return ConfigManager::getStorageBackend(); }

void writeStorageByte(int address, uint8_t value) { activeStorage()->update(address, value); }
} // namespace

// Ensure the manager can resurrect configuration from the backup copy after
// the primary storage header gets nuked.
void test_eeprom_recovery_after_power_cycle() {
    ConfigManager cfg(NUM_POTS, NUM_BUTTONS);
    cfg.setPotChannel(0, 9);
    cfg.setPotCCNumber(0, 77);
    cfg.saveConfiguration();

    writeStorageByte(EEPROM_MAGIC_ADDRESS + 2, (EEPROM_MAGIC_BACKUP >> 8) & 0xFF);
    writeStorageByte(EEPROM_MAGIC_ADDRESS + 3, EEPROM_MAGIC_BACKUP & 0xFF);
    writeStorageByte(EEPROM_BACKUP_START + EEPROM_POT_CHANNELS, 9);
    writeStorageByte(EEPROM_BACKUP_START + EEPROM_POT_CC, 77);
    writeStorageByte(EEPROM_MAGIC_ADDRESS, 0x00);
    writeStorageByte(EEPROM_MAGIC_ADDRESS + 1, 0x00);

    ConfigManager rebooted(NUM_POTS, NUM_BUTTONS);
    std::vector<uint8_t> pots;
    const bool ok = rebooted.loadConfiguration(pots);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_UINT8(9, pots[0]);
    TEST_ASSERT_EQUAL_UINT8(77, rebooted.getPotCCNumber(0));
}

// Baseline calibration should make it through a simulated reboot.
void test_calibration_offsets_survive_power_cycle() {
    auto pm = createPotentiometerManager();
    std::vector<EnvelopeFollower> envs = {EnvelopeFollower(A0, &pm, 0)};
    std::map<int, MIDISlot::EfSettings> mapping;
    MIDISlot::EfSettings settings;
    settings.followerIndex = 0;
    mapping.emplace(0, settings);

    envs[0].setBaseline(0.42f);
    ConfigManager cfg(NUM_POTS, NUM_BUTTONS);
    cfg.saveEnvelopeSettings(mapping, envs);

    for (int i = 0; i < NUM_ENVELOPES; ++i) envelopeConfig.baselines[i] = 0.0f;
    std::vector<EnvelopeFollower> fresh = {EnvelopeFollower(A0, &pm, 0)};
    std::map<int, MIDISlot::EfSettings> reloadedMapping;
    const bool ok = cfg.loadEnvelopeSettings(reloadedMapping, fresh);

    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.42f, fresh[0].getBaseline());
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.42f, envelopeConfig.baselines[0]);
}

void test_high_index_envelope_assignment_survives_reload() {
    auto pm = createPotentiometerManager();
    auto envs = createEnvelopeFollowers(&pm);

    for (size_t i = 0; i < envs.size(); ++i) {
        envs[i].setBaseline(0.1f * static_cast<float>(i + 1));
    }

    std::map<int, MIDISlot::EfSettings> mapping;
    const int highPot = NUM_POTS - 1;
    const int assignedEnv = static_cast<int>(envs.size()) - 1;
    MIDISlot::EfSettings assignedSettings{};
    assignedSettings.followerIndex = static_cast<int8_t>(assignedEnv);
    mapping.emplace(highPot, assignedSettings);

    ConfigManager cfg(NUM_POTS, NUM_BUTTONS);
    cfg.saveEnvelopeSettings(mapping, envs);

    mapping.clear();
    auto reloaded = createEnvelopeFollowers(&pm);
    const bool ok = cfg.loadEnvelopeSettings(mapping, reloaded);

    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_UINT(1u, mapping.size());
    const auto highPotIt = mapping.find(highPot);
    TEST_ASSERT_TRUE(highPotIt != mapping.end());
    TEST_ASSERT_EQUAL_INT(assignedEnv, highPotIt->second.followerIndex);

    if (NUM_POTS > 1) {
        const int unassignedPot = (highPot == 0) ? 1 : 0;
        TEST_ASSERT_TRUE(mapping.find(unassignedPot) == mapping.end());
    }
}

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
