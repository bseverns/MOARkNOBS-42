#include "unity_config.h"
#include <unity.h>

#include "ConfigManager.h"
#include "ProfileModulationStorage.h"
#include "storage/StorageBackend.h"

#include <vector>

namespace {

class MemoryStorageBackend final : public StorageBackend {
  public:
    MemoryStorageBackend()
        : bytes_(EEPROM_PROFILE_MODULATION_START(NUM_PROFILES) + 64U,
                 0x00) {}

    uint16_t length() const override { return static_cast<uint16_t>(bytes_.size()); }

    uint8_t read(int address) const override {
        if (address < 0 || static_cast<size_t>(address) >= bytes_.size()) {
            return 0;
        }
        return bytes_[static_cast<size_t>(address)];
    }

    bool update(int address, uint8_t value) override {
        if (address < 0 || static_cast<size_t>(address) >= bytes_.size()) {
            return false;
        }
        if (dropPrimaryWrites_ && isBlockedPrimaryAddress(address)) {
            return false;
        }
        bytes_[static_cast<size_t>(address)] = value;
        return true;
    }

    void readBytes(int address, void *dest, size_t len) const override {
        auto *out = static_cast<uint8_t *>(dest);
        for (size_t i = 0; i < len; ++i) {
            out[i] = read(address + static_cast<int>(i));
        }
    }

    bool writeBytes(int address, const void *src, size_t len) override {
        const auto *in = static_cast<const uint8_t *>(src);
        bool written = true;
        for (size_t i = 0; i < len; ++i) {
            written = update(address + static_cast<int>(i), in[i]) && written;
        }
        return written;
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

    void corruptPrimaryPotCc(uint16_t base, uint8_t index, uint8_t value) {
        update(base + EEPROM_START_ADDRESS + EEPROM_POT_CC + index, value);
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

void test_config_load_restores_from_backup_when_primary_crc_is_invalid() {
    MemoryStorageBackend storage;
    ConfigManager::setStorageBackend(&storage);

    ConfigManager cfg(NUM_POTS, NUM_BUTTONS);
    configureStoredValues(cfg, 6, 81);
    cfg.saveConfiguration();
    storage.corruptPrimaryPotCc(EEPROM_PROFILE_START(0), 0, 12);

    ConfigManager rebooted(NUM_POTS, NUM_BUTTONS);
    std::vector<uint8_t> pots;
    TEST_ASSERT_TRUE(rebooted.loadConfiguration(pots));
    TEST_ASSERT_EQUAL_UINT8(6, pots[0]);
    TEST_ASSERT_EQUAL_UINT8(81, rebooted.getPotCCNumber(0));
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

void test_slot_lfo_lanes_round_trip_through_slot_storage() {
    MemoryStorageBackend storage;
    ConfigManager::setStorageBackend(&storage);

    ConfigManager cfg(NUM_POTS, NUM_BUTTONS);
    MIDISlot slot{};
    slot.midiChannel = 3;
    slot.lfo.lfo[0].setEnabled(true);
    slot.lfo.lfo[0].setMode(ModCombineMode::Centered);
    slot.lfo.lfo[0].amount = 35;
    slot.lfo.lfo[1].setEnabled(true);
    slot.lfo.lfo[1].setMode(ModCombineMode::Scale);
    slot.lfo.lfo[1].amount = -12;
    cfg.saveSlot(7, slot);

    MIDISlot restored{};
    cfg.loadSlot(7, restored);
    TEST_ASSERT_TRUE(restored.lfo.lfo[0].enabled());
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ModCombineMode::Centered),
                            static_cast<uint8_t>(restored.lfo.lfo[0].mode()));
    TEST_ASSERT_EQUAL_INT8(35, restored.lfo.lfo[0].amount);
    TEST_ASSERT_TRUE(restored.lfo.lfo[1].enabled());
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ModCombineMode::Scale),
                            static_cast<uint8_t>(restored.lfo.lfo[1].mode()));
    TEST_ASSERT_EQUAL_INT8(-12, restored.lfo.lfo[1].amount);

    ConfigManager::setStorageBackend(nullptr);
}

void test_profile_modulation_round_trip_preserves_arg_and_lfo_lanes() {
    MemoryStorageBackend storage;
    ConfigManager::setStorageBackend(&storage);
    ConfigManager cfg(NUM_POTS, NUM_BUTTONS);

    ProfileModulationExtension saved{};
    SlotARGConfig arg{};
    arg.enabled = 1;
    arg.method = ARGMethod::XORR;
    arg.sourceA = 4;
    arg.sourceB = 2;
    saved.slots[9].argPacked = packProfileSlotArg(arg);
    saved.slots[9].lfo[0].setEnabled(true);
    saved.slots[9].lfo[0].setMode(ModCombineMode::Centered);
    saved.slots[9].lfo[0].amount = 61;
    saved.slots[9].lfo[1].setEnabled(true);
    saved.slots[9].lfo[1].setMode(ModCombineMode::Scale);
    saved.slots[9].lfo[1].amount = -37;

    TEST_ASSERT_TRUE(cfg.saveProfileModulation(2, saved));
    ProfileModulationExtension restored{};
    TEST_ASSERT_TRUE(cfg.loadProfileModulation(2, restored));
    const SlotARGConfig restoredArg = unpackProfileSlotArg(restored.slots[9].argPacked);
    TEST_ASSERT_TRUE(restoredArg.enabled);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ARGMethod::XORR),
                            static_cast<uint8_t>(restoredArg.method));
    TEST_ASSERT_EQUAL_UINT8(4, restoredArg.sourceA);
    TEST_ASSERT_EQUAL_UINT8(2, restoredArg.sourceB);
    TEST_ASSERT_EQUAL_INT8(61, restored.slots[9].lfo[0].amount);
    TEST_ASSERT_EQUAL_INT8(-37, restored.slots[9].lfo[1].amount);

    ConfigManager::setStorageBackend(nullptr);
}

void test_schema7_migration_relocates_macro_and_scene_tail() {
    MemoryStorageBackend storage;
    ConfigManager::setStorageBackend(&storage);
    const uint16_t schema7 = 0x0007;
    storage.writeBytes(EEPROM_CONFIG_VERSION, &schema7, sizeof(schema7));
    const size_t oldMacroStorage = EEPROM_PROFILE_MODULATION_BASE;
    const size_t newMacroStorage = EEPROM_PROFILE_MODULATION_START(NUM_PROFILES);
    storage.update(static_cast<int>(oldMacroStorage + 17), 0xA5);

    ConfigManager cfg(NUM_POTS, NUM_BUTTONS);
    std::vector<uint8_t> pots;
    cfg.begin(pots);

    TEST_ASSERT_EQUAL_UINT8(0xA5, storage.read(static_cast<int>(newMacroStorage + 17)));
    TEST_ASSERT_EQUAL_UINT8(0x00, storage.read(static_cast<int>(oldMacroStorage + 17)));
    uint16_t storedVersion = 0;
    storage.readBytes(EEPROM_CONFIG_VERSION, &storedVersion, sizeof(storedVersion));
    TEST_ASSERT_EQUAL_UINT16(CONFIG_VERSION, storedVersion);

    ConfigManager::setStorageBackend(nullptr);
}

void test_schema6_slot_migration_preserves_downstream_profile_bytes() {
    struct LegacyMIDISlotV6 {
        MIDIMessageType type = MIDIMessageType::OFF;
        uint8_t midiChannel = 1;
        uint8_t data1 = 0;
        bool active = false;
        uint8_t arpNote = 0;
        uint8_t sysexLength = 0;
        MIDISlot::EfSettings efSettings{};
        std::array<uint8_t, SysExTemplate::kMaxLength> sysexTemplate{};
        MIDISlot::EfRuntime ef{};
        SlotARGConfig arg{};
    };

    MemoryStorageBackend storage;
    ConfigManager::setStorageBackend(&storage);
    const uint16_t schema6 = 0x0006;
    storage.writeBytes(EEPROM_CONFIG_VERSION, &schema6, sizeof(schema6));
    for (uint8_t i = 0; i < NUM_SLOTS; ++i) {
        LegacyMIDISlotV6 legacy{};
        legacy.data1 = i;
        legacy.efSettings.followerIndex = -1;
        legacy.ef.followerIndex = -1;
        const int address = static_cast<int>(
            EEPROM_SLOT_BASE + static_cast<size_t>(i) * sizeof(LegacyMIDISlotV6));
        storage.writeBytes(address, &legacy, sizeof(legacy));
    }

    const size_t oldTail = EEPROM_SLOT_BASE + sizeof(LegacyMIDISlotV6) * NUM_SLOTS +
                           sizeof(float) * 2 + sizeof(uint16_t);
    const size_t oldProfileSettings = oldTail + NUM_PROFILES * EEPROM_PROFILE_BLOCK_SIZE;
    storage.update(static_cast<int>(oldProfileSettings + 17), 0xA5);

    ConfigManager cfg(NUM_POTS, NUM_BUTTONS);
    std::vector<uint8_t> pots;
    cfg.begin(pots);

    TEST_ASSERT_EQUAL_UINT8(0xA5, storage.read(EEPROM_PROFILE_SETTINGS_BASE + 17));
    TEST_ASSERT_EQUAL_UINT8(5, cfg.getSlot(5).data1);
    TEST_ASSERT_FALSE(cfg.getSlot(5).lfo.lfo[0].enabled());
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ModCombineMode::Centered),
                            static_cast<uint8_t>(cfg.getSlot(5).lfo.lfo[0].mode()));

    ConfigManager::setStorageBackend(nullptr);
}
