#include "unity_config.h"
#include <unity.h>

#include "ConfigManager.h"
#include "LFO/LFOManager.h"
#include "ProfileModulationStorage.h"
#include "SchemaMigrationLayout.h"
#include "protocol/SceneStorage.h"
#include "storage/StorageBackend.h"

#include <array>
#include <cstring>
#include <vector>

namespace {

class MemoryStorageBackend final : public StorageBackend {
  public:
    MemoryStorageBackend()
        : bytes_(SceneStorage::kRequiredStorageBytes + 64U, 0x00) {}

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
        if (address == failedWriteAddress_) return false;
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

    void setFailedWriteAddress(int address) { failedWriteAddress_ = address; }

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
    int failedWriteAddress_ = -1;
};

void configureStoredValues(ConfigManager &cfg, uint8_t channel, uint8_t cc) {
    cfg.setPotChannel(0, channel);
    cfg.setPotCCNumber(0, cc);
}

struct LegacyMIDISlotV6Fixture {
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

struct LegacyConfigStateV6Fixture {
    uint16_t version = 1;
    std::array<uint8_t, NUM_POTS> potChannels{};
    std::array<uint8_t, NUM_POTS> potCCNumbers{};
    std::array<LegacyMIDISlotV6Fixture, NUM_SLOTS> slots{};
    uint8_t argEnabled = 0;
    uint8_t argMethod = 0;
    uint8_t argSourceA = 0;
    uint8_t argSourceB = 1;
    uint8_t envelopeMode = 0;
    uint8_t ledBrightness = 255;
    uint8_t ledMode = 0;
    uint8_t ledR = 0;
    uint8_t ledG = 0;
    uint8_t ledB = 0;
    uint8_t filterType = 0;
    float filterFrequency = 20.0f;
    float filterQ = 1.0f;
    std::array<float, NUM_ENVELOPES> baselines{};
};

struct LegacyMacroRecordV6Fixture {
    uint16_t version = 1;
    uint16_t crc = 0;
    uint8_t occupied = 0;
    LegacyConfigStateV6Fixture state{};
};

struct LegacySceneRecordV6Fixture {
    uint16_t version = 1;
    uint16_t crc = 0;
    uint8_t occupied = 0;
    char name[16] = {0};
    LegacyConfigStateV6Fixture state{};
};

struct __attribute__((packed)) LegacyProfileModulationV1Fixture {
    uint16_t version = 0x0001;
    uint16_t crc = 0;
    ProfileSlotModSettings slots[NUM_SLOTS]{};
};

uint16_t fixtureModulationCrcUpdate(uint16_t crc, uint8_t data) {
    crc ^= data;
    for (uint8_t i = 0; i < 8; ++i) {
        crc = (crc & 1) ? static_cast<uint16_t>((crc >> 1) ^ 0xA001)
                        : static_cast<uint16_t>(crc >> 1);
    }
    return crc;
}

uint16_t fixtureLegacyModulationCrc(const LegacyProfileModulationV1Fixture &record) {
    const auto *bytes = reinterpret_cast<const uint8_t *>(&record);
    uint16_t crc = 0xFFFF;
    for (size_t i = offsetof(LegacyProfileModulationV1Fixture, slots); i < sizeof(record); ++i) {
        crc = fixtureModulationCrcUpdate(crc, bytes[i]);
    }
    return crc;
}

uint16_t fixtureSceneCrcUpdate(uint16_t crc, uint8_t data) {
    crc ^= static_cast<uint16_t>(data) << 8;
    for (uint8_t i = 0; i < 8; ++i) {
        crc = (crc & 0x8000) ? static_cast<uint16_t>((crc << 1) ^ 0x1021)
                             : static_cast<uint16_t>(crc << 1);
    }
    return crc;
}

template <typename T> uint16_t fixtureSceneRecordCrc(const T &record) {
    const auto *bytes = reinterpret_cast<const uint8_t *>(&record);
    uint16_t crc = 0xFFFF;
    constexpr size_t payloadOffset = sizeof(record.version) + sizeof(record.crc);
    for (size_t i = payloadOffset; i < sizeof(T); ++i) {
        crc = fixtureSceneCrcUpdate(crc, bytes[i]);
    }
    return crc;
}

LegacyConfigStateV6Fixture legacySceneState(uint8_t marker) {
    LegacyConfigStateV6Fixture state{};
    state.potChannels[0] = static_cast<uint8_t>((marker % 16) + 1);
    state.potCCNumbers[0] = marker;
    state.slots[2].type = MIDIMessageType::CC;
    state.slots[2].active = true;
    state.slots[2].data1 = marker;
    state.slots[2].efSettings.followerIndex = -1;
    state.slots[2].ef.followerIndex = -1;
    state.ledBrightness = marker;
    state.filterFrequency = 1000.0f + marker;
    return state;
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

void test_profile_load_retains_current_config_when_both_copies_are_corrupt() {
    MemoryStorageBackend storage;
    storage.fill(0x00);
    ConfigManager::setStorageBackend(&storage);

    ConfigManager cfg(NUM_POTS, NUM_BUTTONS);
    configureStoredValues(cfg, 11, 93);

    TEST_ASSERT_FALSE(cfg.loadProfile(2));
    TEST_ASSERT_EQUAL_UINT8(11, cfg.getPotChannel(0));
    TEST_ASSERT_EQUAL_UINT8(93, cfg.getPotCCNumber(0));

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
    saved.midiInputBindingCount = 1;
    saved.midiInputBindings[0].port = static_cast<uint8_t>(MidiInputPort::Usb);
    saved.midiInputBindings[0].channel = 7;
    saved.midiInputBindings[0].controller = 74;
    saved.midiInputBindings[0].target =
        static_cast<uint8_t>(MachineParameterTarget::ArpSwing);
    saved.midiInputBindings[0].flags = MIDI_INPUT_FLAG_SOFT_TAKEOVER;

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
    TEST_ASSERT_EQUAL_UINT8(1, restored.midiInputBindingCount);
    TEST_ASSERT_EQUAL_UINT8(7, restored.midiInputBindings[0].channel);
    TEST_ASSERT_EQUAL_UINT8(74, restored.midiInputBindings[0].controller);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(MachineParameterTarget::ArpSwing),
                            restored.midiInputBindings[0].target);

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

void test_schema7_migration_failure_does_not_promote_config_version() {
    MemoryStorageBackend storage;
    ConfigManager::setStorageBackend(&storage);
    const uint16_t schema7 = 0x0007;
    storage.writeBytes(EEPROM_CONFIG_VERSION, &schema7, sizeof(schema7));
    constexpr Schema7StorageLayout layout = schema7StorageLayout();
    storage.setFailedWriteAddress(static_cast<int>(layout.newMacroStorage + 17U));

    ConfigManager cfg(NUM_POTS, NUM_BUTTONS);
    std::vector<uint8_t> pots;
    cfg.begin(pots);

    uint16_t storedVersion = 0;
    storage.readBytes(EEPROM_CONFIG_VERSION, &storedVersion, sizeof(storedVersion));
    TEST_ASSERT_EQUAL_UINT16(schema7, storedVersion);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(ConfigManager::MigrationResult::WriteFailure),
        static_cast<uint8_t>(cfg.getLastMigrationResult()));

    ConfigManager::setStorageBackend(nullptr);
}

FLASHMEM void test_schema8_migration_preserves_slots_profiles_modulation_and_downstream_data() {
    MemoryStorageBackend storage;
    ConfigManager::setStorageBackend(&storage);
    ConfigManager cfg(NUM_POTS, NUM_BUTTONS);

    configureStoredValues(cfg, 5, 71);
    cfg.setARGEnable(1);
    cfg.setARGMethod(static_cast<uint8_t>(ARGMethod::XORR));
    cfg.setEnvelopePair(4, 2);
    cfg.setActiveProfile(1);

    configureStoredValues(cfg, 11, 93);
    cfg.saveProfile(1);

    MIDISlot savedSlot{};
    savedSlot.type = MIDIMessageType::CC;
    savedSlot.midiChannel = 7;
    savedSlot.data1 = 74;
    savedSlot.active = true;
    savedSlot.arpNote = 48;
    cfg.saveSlot(0, savedSlot);

    ProfileData savedProfile{};
    savedProfile.led.brightness = 93;
    savedProfile.slots[5].midiChannel = 12;
    TEST_ASSERT_TRUE(cfg.saveProfileSettings(2, savedProfile));

    LegacyProfileModulationV1Fixture legacyModulation{};
    SlotARGConfig arg{};
    arg.enabled = 1;
    arg.method = ARGMethod::XORR;
    arg.sourceA = 4;
    arg.sourceB = 2;
    legacyModulation.slots[5].argPacked = packProfileSlotArg(arg);
    legacyModulation.slots[5].lfo[1].setEnabled(true);
    legacyModulation.slots[5].lfo[1].setMode(ModCombineMode::Scale);
    legacyModulation.slots[5].lfo[1].amount = -37;
    legacyModulation.crc = fixtureLegacyModulationCrc(legacyModulation);
    storage.writeBytes(EEPROM_PROFILE_MODULATION_START(2), &legacyModulation,
                       sizeof(legacyModulation));

    constexpr size_t sentinelAddress = SceneStorage::kRequiredStorageBytes + 23U;
    storage.update(static_cast<int>(sentinelAddress), 0xD7);
    const uint16_t schema8 = 0x0008;
    for (uint8_t id = 0; id < NUM_PROFILES; ++id) {
        const uint16_t base = EEPROM_PROFILE_START(id);
        storage.writeBytes(base + EEPROM_CONFIG_VERSION, &schema8, sizeof(schema8));
        storage.writeBytes(base + EEPROM_BACKUP_START + EEPROM_CONFIG_VERSION, &schema8,
                           sizeof(schema8));
    }

    std::vector<uint8_t> pots;
    ConfigManager rebooted(NUM_POTS, NUM_BUTTONS);
    rebooted.begin(pots);

    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(ConfigManager::MigrationResult::Success),
        static_cast<uint8_t>(rebooted.getLastMigrationResult()));
    for (uint8_t id = 0; id < NUM_PROFILES; ++id) {
        const uint16_t base = EEPROM_PROFILE_START(id);
        uint16_t primaryVersion = 0;
        uint16_t backupVersion = 0;
        storage.readBytes(base + EEPROM_CONFIG_VERSION, &primaryVersion,
                          sizeof(primaryVersion));
        storage.readBytes(base + EEPROM_BACKUP_START + EEPROM_CONFIG_VERSION, &backupVersion,
                          sizeof(backupVersion));
        TEST_ASSERT_EQUAL_UINT16(CONFIG_VERSION, primaryVersion);
        TEST_ASSERT_EQUAL_UINT16(CONFIG_VERSION, backupVersion);
    }
    TEST_ASSERT_EQUAL_UINT8(1, rebooted.getActiveProfile());
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ARGMethod::XORR), rebooted.getARGMethod());
    TEST_ASSERT_EQUAL_UINT8(1, rebooted.getARGEnable());
    TEST_ASSERT_EQUAL_UINT8(7, rebooted.getSlot(0).midiChannel);
    TEST_ASSERT_EQUAL_UINT8(74, rebooted.getSlot(0).data1);
    TEST_ASSERT_TRUE(rebooted.getSlot(0).active);

    TEST_ASSERT_TRUE(rebooted.loadProfile(1));
    TEST_ASSERT_EQUAL_UINT8(11, rebooted.getPotChannel(0));
    TEST_ASSERT_EQUAL_UINT8(93, rebooted.getPotCCNumber(0));

    storage.corruptPrimaryPotCc(EEPROM_PROFILE_START(1), 0, 12);
    rebooted.setPotChannelLive(0, 2);
    rebooted.setPotCCNumberLive(0, 3);
    TEST_ASSERT_TRUE(rebooted.loadProfile(1));
    TEST_ASSERT_EQUAL_UINT8(11, rebooted.getPotChannel(0));
    TEST_ASSERT_EQUAL_UINT8(93, rebooted.getPotCCNumber(0));

    ProfileData restoredProfile{};
    TEST_ASSERT_TRUE(rebooted.loadProfileSettings(2, restoredProfile));
    TEST_ASSERT_EQUAL_UINT8(93, restoredProfile.led.brightness);
    TEST_ASSERT_EQUAL_UINT8(12, restoredProfile.slots[5].midiChannel);

    ProfileModulationExtension restoredModulation{};
    TEST_ASSERT_TRUE(rebooted.loadProfileModulation(2, restoredModulation));
    const SlotARGConfig restoredArg =
        unpackProfileSlotArg(restoredModulation.slots[5].argPacked);
    TEST_ASSERT_TRUE(restoredArg.enabled);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ARGMethod::XORR),
                            static_cast<uint8_t>(restoredArg.method));
    TEST_ASSERT_EQUAL_UINT8(4, restoredArg.sourceA);
    TEST_ASSERT_EQUAL_UINT8(2, restoredArg.sourceB);
    TEST_ASSERT_TRUE(restoredModulation.slots[5].lfo[1].enabled());
    TEST_ASSERT_EQUAL_INT8(-37, restoredModulation.slots[5].lfo[1].amount);
    TEST_ASSERT_EQUAL_UINT8(0, restoredModulation.midiInputBindingCount);
    TEST_ASSERT_EQUAL_UINT8(0xD7, storage.read(static_cast<int>(sentinelAddress)));

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

void test_schema6_relocation_failure_does_not_promote_config_version() {
    MemoryStorageBackend storage;
    ConfigManager::setStorageBackend(&storage);
    const uint16_t schema6 = 0x0006;
    storage.writeBytes(EEPROM_CONFIG_VERSION, &schema6, sizeof(schema6));
    constexpr Schema6StorageLayout layout = schema6StorageLayout(
        sizeof(LegacyMIDISlotV6Fixture), sizeof(LegacyMacroRecordV6Fixture),
        sizeof(LegacySceneRecordV6Fixture), SceneStorage::kSceneSlotCount);
    storage.setFailedWriteAddress(static_cast<int>(layout.newConfigTail + 17U));

    ConfigManager cfg(NUM_POTS, NUM_BUTTONS);
    std::vector<uint8_t> pots;
    cfg.begin(pots);

    uint16_t storedVersion = 0;
    storage.readBytes(EEPROM_CONFIG_VERSION, &storedVersion, sizeof(storedVersion));
    TEST_ASSERT_EQUAL_UINT16(schema6, storedVersion);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(ConfigManager::MigrationResult::WriteFailure),
        static_cast<uint8_t>(cfg.getLastMigrationResult()));

    ConfigManager::setStorageBackend(nullptr);
}

void test_schema6_direct_migration_preserves_profiles_macro_and_scenes() {
    MemoryStorageBackend storage;
    ConfigManager::setStorageBackend(&storage);
    ConfigManager cfg(NUM_POTS, NUM_BUTTONS);

    // Produce four valid profile payloads with the current profile codec, then
    // place their bytes at the schema-6 addresses used before slot expansion.
    std::vector<uint8_t> profileBlocks(
        static_cast<size_t>(NUM_PROFILES) * EEPROM_PROFILE_SETTINGS_BLOCK_SIZE);
    for (uint8_t id = 0; id < NUM_PROFILES; ++id) {
        ProfileData profile{};
        profile.led.brightness = static_cast<uint8_t>(40 + id);
        profile.slots[id].midiChannel = static_cast<uint8_t>(id + 1);
        profile.routeCount = 1;
        profile.routes[0].type =
            static_cast<uint8_t>(LFOManager::Route::Type::SlotValue);
        profile.routes[0].lfoIndex = static_cast<uint8_t>(id % PROFILE_LFO_COUNT);
        profile.routes[0].depth = 0.25f + static_cast<float>(id) * 0.1f;
        profile.routes[0].target = static_cast<uint8_t>(10 + id);
        profile.routes[0].amount = static_cast<int8_t>(60 - id);
        TEST_ASSERT_TRUE(cfg.saveProfileSettings(id, profile));
        storage.readBytes(EEPROM_PROFILE_SETTINGS_START(id),
                          profileBlocks.data() +
                              static_cast<size_t>(id) * EEPROM_PROFILE_SETTINGS_BLOCK_SIZE,
                          EEPROM_PROFILE_SETTINGS_BLOCK_SIZE);
        profileBlocks[(static_cast<size_t>(id) + 1) * EEPROM_PROFILE_SETTINGS_BLOCK_SIZE - 1] =
            static_cast<uint8_t>(0xC0 + id);
    }

    // Remove the current-address copies so the assertions below can only pass
    // if migration relocates the schema-6 source blocks successfully.
    std::vector<uint8_t> emptyProfileBlocks(profileBlocks.size(), 0x00);
    storage.writeBytes(EEPROM_PROFILE_SETTINGS_START(0), emptyProfileBlocks.data(),
                       emptyProfileBlocks.size());

    constexpr size_t oldSlotRegionBytes = sizeof(LegacyMIDISlotV6Fixture) * NUM_SLOTS;
    const size_t oldConfigTail = EEPROM_SLOT_BASE + oldSlotRegionBytes + sizeof(float) * 2 +
                                 sizeof(uint16_t);
    const size_t oldProfileSettings = oldConfigTail +
                                      NUM_PROFILES * EEPROM_PROFILE_BLOCK_SIZE;
    for (uint8_t id = 0; id < NUM_PROFILES; ++id) {
        storage.writeBytes(static_cast<int>(oldProfileSettings +
                                            id * EEPROM_PROFILE_SETTINGS_BLOCK_SIZE),
                           profileBlocks.data() +
                               static_cast<size_t>(id) * EEPROM_PROFILE_SETTINGS_BLOCK_SIZE,
                           EEPROM_PROFILE_SETTINGS_BLOCK_SIZE);
    }

    for (uint8_t i = 0; i < NUM_SLOTS; ++i) {
        LegacyMIDISlotV6Fixture legacy{};
        legacy.data1 = i;
        legacy.efSettings.followerIndex = -1;
        legacy.ef.followerIndex = -1;
        storage.writeBytes(static_cast<int>(EEPROM_SLOT_BASE +
                                            static_cast<size_t>(i) * sizeof(legacy)),
                           &legacy, sizeof(legacy));
    }

    const size_t oldMacroStorage = oldProfileSettings +
                                   NUM_PROFILES * EEPROM_PROFILE_SETTINGS_BLOCK_SIZE;
    auto *macro = new LegacyMacroRecordV6Fixture{};
    TEST_ASSERT_NOT_NULL(macro);
    macro->occupied = 1;
    macro->state = legacySceneState(91);
    macro->crc = fixtureSceneRecordCrc(*macro);
    storage.writeBytes(static_cast<int>(oldMacroStorage), macro, sizeof(*macro));

    const size_t oldSceneStorage = oldMacroStorage + sizeof(*macro);
    delete macro;
    for (uint8_t slot = 0; slot < SceneStorage::kSceneSlotCount; ++slot) {
        auto *scene = new LegacySceneRecordV6Fixture{};
        TEST_ASSERT_NOT_NULL(scene);
        scene->occupied = 1;
        const char name[] = "Schema6";
        std::memcpy(scene->name, name, sizeof(name));
        scene->name[7] = static_cast<char>('0' + slot);
        scene->state = legacySceneState(static_cast<uint8_t>(70 + slot));
        scene->crc = fixtureSceneRecordCrc(*scene);
        storage.writeBytes(static_cast<int>(oldSceneStorage +
                                            static_cast<size_t>(slot) * sizeof(*scene)),
                           scene, sizeof(*scene));
        delete scene;
    }

    // Guard the first bytes outside the current schema-8 layout. A migration
    // must relocate only known records and leave unrelated storage untouched.
    constexpr size_t guardBytes = 32;
    for (size_t offset = 0; offset < guardBytes; ++offset) {
        storage.update(static_cast<int>(SceneStorage::kRequiredStorageBytes + offset),
                       static_cast<uint8_t>(0x80 + offset));
    }

    const uint16_t schema6 = 0x0006;
    storage.writeBytes(EEPROM_CONFIG_VERSION, &schema6, sizeof(schema6));
    std::vector<uint8_t> pots;
    cfg.begin(pots);

    for (uint8_t id = 0; id < NUM_PROFILES; ++id) {
        ProfileData restored{};
        TEST_ASSERT_TRUE(cfg.loadProfileSettings(id, restored));
        TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(40 + id), restored.led.brightness);
        TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(id + 1),
                                restored.slots[id].midiChannel);
        TEST_ASSERT_EQUAL_UINT8(1, restored.routeCount);
        TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(id % PROFILE_LFO_COUNT),
                                restored.routes[0].lfoIndex);
        TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(10 + id), restored.routes[0].target);
        TEST_ASSERT_EQUAL_INT8(static_cast<int8_t>(60 - id), restored.routes[0].amount);
        TEST_ASSERT_EQUAL_UINT8(
            static_cast<uint8_t>(0xC0 + id),
            storage.read(EEPROM_PROFILE_SETTINGS_START(id) +
                         EEPROM_PROFILE_SETTINGS_BLOCK_SIZE - 1));
        ProfileModulationExtension modulation{};
        TEST_ASSERT_FALSE(cfg.loadProfileModulation(id, modulation));
        TEST_ASSERT_EQUAL_UINT8(0, storage.read(EEPROM_PROFILE_MODULATION_START(id)));
        TEST_ASSERT_EQUAL_UINT8(
            0, storage.read(EEPROM_PROFILE_MODULATION_START(id) +
                            EEPROM_PROFILE_MODULATION_BLOCK_SIZE - 1));
    }

    SceneStorage::ConfigState restoredMacro{};
    TEST_ASSERT_TRUE(SceneStorage::loadMacroSnapshot(restoredMacro));
    TEST_ASSERT_EQUAL_UINT8(91, restoredMacro.slots[2].data1);
    TEST_ASSERT_FALSE(restoredMacro.slots[2].lfo.lfo[0].enabled());

    for (uint8_t slot = 0; slot < SceneStorage::kSceneSlotCount; ++slot) {
        SceneStorage::SceneEntry restored{};
        TEST_ASSERT_TRUE(SceneStorage::loadSceneSlot(slot, restored));
        TEST_ASSERT_EQUAL_CHAR(static_cast<char>('0' + slot), restored.name[7]);
        TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(70 + slot),
                                restored.state.slots[2].data1);
        TEST_ASSERT_FALSE(restored.state.slots[2].lfo.lfo[0].enabled());
    }

    TEST_ASSERT_EQUAL_UINT32(EEPROM_PROFILE_MODULATION_BASE,
                             EEPROM_PROFILE_SETTINGS_START(NUM_PROFILES));
    TEST_ASSERT_EQUAL_UINT32(SceneStorage::kMacroStorageAddress,
                             EEPROM_PROFILE_MODULATION_START(NUM_PROFILES));
    TEST_ASSERT_EQUAL_UINT32(SceneStorage::kSceneStorageBase,
                             SceneStorage::kMacroStorageAddress +
                                 SceneStorage::kMacroRecordBytes);
    TEST_ASSERT_EQUAL_UINT32(
        SceneStorage::kRequiredStorageBytes,
        SceneStorage::kSceneStorageBase +
            static_cast<size_t>(SceneStorage::kSceneSlotCount) *
                SceneStorage::kSceneRecordBytes);

    // Exercise the last profile blocks adjacent to macro storage, then prove
    // those writes cannot invalidate the migrated macro or any scene.
    ProfileData rewritten{};
    TEST_ASSERT_TRUE(cfg.loadProfileSettings(NUM_PROFILES - 1, rewritten));
    rewritten.led.brightness = 33;
    TEST_ASSERT_TRUE(cfg.saveProfileSettings(NUM_PROFILES - 1, rewritten));
    ProfileModulationExtension newModulation{};
    newModulation.slots[NUM_SLOTS - 1].argPacked = 0x1234;
    newModulation.slots[NUM_SLOTS - 1].lfo[1].setEnabled(true);
    newModulation.slots[NUM_SLOTS - 1].lfo[1].amount = -41;
    TEST_ASSERT_TRUE(cfg.saveProfileModulation(NUM_PROFILES - 1, newModulation));

    TEST_ASSERT_TRUE(SceneStorage::loadMacroSnapshot(restoredMacro));
    TEST_ASSERT_EQUAL_UINT8(91, restoredMacro.slots[2].data1);
    for (uint8_t slot = 0; slot < SceneStorage::kSceneSlotCount; ++slot) {
        SceneStorage::SceneEntry restored{};
        TEST_ASSERT_TRUE(SceneStorage::loadSceneSlot(slot, restored));
        TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(70 + slot),
                                restored.state.slots[2].data1);
    }
    for (size_t offset = 0; offset < guardBytes; ++offset) {
        TEST_ASSERT_EQUAL_UINT8(
            static_cast<uint8_t>(0x80 + offset),
            storage.read(static_cast<int>(SceneStorage::kRequiredStorageBytes + offset)));
    }

    uint16_t storedVersion = 0;
    storage.readBytes(EEPROM_CONFIG_VERSION, &storedVersion, sizeof(storedVersion));
    TEST_ASSERT_EQUAL_UINT16(CONFIG_VERSION, storedVersion);
    ConfigManager::setStorageBackend(nullptr);
}
