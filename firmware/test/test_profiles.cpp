#include "unity_config.h"
#include <unity.h>

#include "ConfigManager.h"
#include "EnvelopeFollower.h"
#include <EEPROM.h>

namespace {
void clearProfileSettingsBlock(uint8_t id) {
    // Scrub the target profile block so CRC checks are deterministic.
    const uint16_t base = EEPROM_PROFILE_SETTINGS_START(id);
    for (uint16_t offset = 0; offset < EEPROM_PROFILE_SETTINGS_BLOCK_SIZE; ++offset) {
        EEPROM.update(static_cast<int>(base + offset), 0);
    }
}
} // namespace

void test_profile_crc_rejects_corruption() {
    // Corrupt a byte after saving and verify the CRC check fails.
    ConfigManager cfg(NUM_POTS, NUM_BUTTONS);
    clearProfileSettingsBlock(0);

    ProfileData profile{};
    profile.routeCount = 1;
    profile.routes[0].type = 1;
    TEST_ASSERT_TRUE(cfg.saveProfileSettings(0, profile));

    const uint16_t base = EEPROM_PROFILE_SETTINGS_START(0);
    uint8_t byte = EEPROM.read(base + sizeof(uint16_t));
    EEPROM.update(base + sizeof(uint16_t), static_cast<uint8_t>(byte ^ 0xFF));

    ProfileData loaded{};
    TEST_ASSERT_FALSE(cfg.loadProfileSettings(0, loaded));
}

void test_profile_bounds_clamp() {
    // Out-of-range fields should be sanitized on load.
    ConfigManager cfg(NUM_POTS, NUM_BUTTONS);
    clearProfileSettingsBlock(1);

    ProfileData profile{};
    profile.routeCount = static_cast<uint8_t>(PROFILE_MAX_ROUTES + 3);
    for (uint8_t i = 0; i < NUM_SLOTS; ++i) {
        profile.slots[i].midiChannel = 0;
        profile.slots[i].ef.mode = 99;
    }
    TEST_ASSERT_TRUE(cfg.saveProfileSettings(1, profile));

    ProfileData loaded{};
    TEST_ASSERT_TRUE(cfg.loadProfileSettings(1, loaded));
    TEST_ASSERT_TRUE(loaded.routeCount <= PROFILE_MAX_ROUTES);
    for (uint8_t i = 0; i < NUM_SLOTS; ++i) {
        TEST_ASSERT_TRUE(loaded.slots[i].midiChannel >= 1);
        TEST_ASSERT_TRUE(loaded.slots[i].midiChannel <= 16);
        TEST_ASSERT_TRUE(loaded.slots[i].ef.mode <= static_cast<uint8_t>(EnvelopeFollower::EFMode::Follower));
    }
}
