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

ProfileData makePopulatedProfile() {
    ProfileData profile{};
    profile.routeCount = 2;
    profile.arp.lengthTicks = 23;
    profile.arp.shape = 4;
    profile.arp.swingPercent = 17;
    profile.arp.gatePercent = 68;
    profile.arp.octaveRange = 2;
    profile.led.brightness = 201;
    profile.led.r = 12;
    profile.led.g = 34;
    profile.led.b = 56;
    profile.lfos[0].shape = 3;
    profile.lfos[0].frequencyHz = 2.5f;
    profile.lfos[0].depth = 0.75f;
    profile.lfos[0].bipolar = 1;
    profile.lfos[0].syncEnabled = 1;
    profile.lfos[0].syncRatio = 6;
    profile.lfos[1].shape = 1;
    profile.lfos[1].frequencyHz = 0.125f;
    profile.lfos[1].depth = 0.35f;
    profile.lfos[1].bipolar = 0;
    profile.lfos[1].syncEnabled = 0;
    profile.lfos[1].syncRatio = 2;

    profile.routes[0].type = 1;
    profile.routes[0].lfoIndex = 0;
    profile.routes[0].depth = 0.8f;
    profile.routes[0].target = 3;
    profile.routes[0].channel = 6;
    profile.routes[0].ccMsb = 74;
    profile.routes[0].ccLsb = 42;
    profile.routes[1].type = 2;
    profile.routes[1].lfoIndex = 1;
    profile.routes[1].depth = 0.4f;
    profile.routes[1].target = 1;
    profile.routes[1].channel = 9;
    profile.routes[1].ccMsb = 14;
    profile.routes[1].ccLsb = 15;

    for (uint8_t i = 0; i < NUM_SLOTS; ++i) {
        profile.slots[i].midiChannel = static_cast<uint8_t>((i % 16) + 1);
        profile.slots[i].ef.mode = static_cast<uint8_t>(
            i % (static_cast<uint8_t>(EnvelopeFollower::EFMode::Follower) + 1));
        profile.slots[i].ef.autoBaseline = static_cast<uint8_t>(i % 2);
        profile.slots[i].ef.autoGain = static_cast<uint8_t>((i + 1) % 2);
        profile.slots[i].ef.gateThreshold = static_cast<uint8_t>(20 + i);
        profile.slots[i].ef.gainTarget = static_cast<uint8_t>(64 + i);
        profile.slots[i].ef.attackMs = static_cast<uint16_t>(10 + i);
        profile.slots[i].ef.releaseMs = static_cast<uint16_t>(30 + i);
    }
    return profile;
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
        profile.slots[i].ef.attackMs = 0;
        profile.slots[i].ef.releaseMs = 65535;
    }
    TEST_ASSERT_TRUE(cfg.saveProfileSettings(1, profile));

    ProfileData loaded{};
    TEST_ASSERT_TRUE(cfg.loadProfileSettings(1, loaded));
    TEST_ASSERT_TRUE(loaded.routeCount <= PROFILE_MAX_ROUTES);
    for (uint8_t i = 0; i < NUM_SLOTS; ++i) {
        TEST_ASSERT_TRUE(loaded.slots[i].midiChannel >= 1);
        TEST_ASSERT_TRUE(loaded.slots[i].midiChannel <= 16);
        TEST_ASSERT_TRUE(loaded.slots[i].ef.mode <=
                         static_cast<uint8_t>(EnvelopeFollower::EFMode::Follower));
        TEST_ASSERT_EQUAL_UINT16(EF_TIME_MIN_MS, loaded.slots[i].ef.attackMs);
        TEST_ASSERT_EQUAL_UINT16(EF_TIME_MAX_MS, loaded.slots[i].ef.releaseMs);
    }
}

void test_profile_round_trip_preserves_profile_payload() {
    ConfigManager cfg(NUM_POTS, NUM_BUTTONS);
    clearProfileSettingsBlock(2);

    const ProfileData saved = makePopulatedProfile();
    TEST_ASSERT_TRUE(cfg.saveProfileSettings(2, saved));

    ProfileData loaded{};
    TEST_ASSERT_TRUE(cfg.loadProfileSettings(2, loaded));

    TEST_ASSERT_EQUAL_UINT8(saved.routeCount, loaded.routeCount);
    TEST_ASSERT_EQUAL_UINT8(saved.arp.lengthTicks, loaded.arp.lengthTicks);
    TEST_ASSERT_EQUAL_UINT8(saved.arp.shape, loaded.arp.shape);
    TEST_ASSERT_EQUAL_UINT8(saved.arp.swingPercent, loaded.arp.swingPercent);
    TEST_ASSERT_EQUAL_UINT8(saved.arp.gatePercent, loaded.arp.gatePercent);
    TEST_ASSERT_EQUAL_UINT8(saved.arp.octaveRange, loaded.arp.octaveRange);
    TEST_ASSERT_EQUAL_UINT8(saved.led.brightness, loaded.led.brightness);
    TEST_ASSERT_EQUAL_UINT8(saved.led.r, loaded.led.r);
    TEST_ASSERT_EQUAL_UINT8(saved.led.g, loaded.led.g);
    TEST_ASSERT_EQUAL_UINT8(saved.led.b, loaded.led.b);
    TEST_ASSERT_EQUAL_UINT8(saved.lfos[0].shape, loaded.lfos[0].shape);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, saved.lfos[0].frequencyHz, loaded.lfos[0].frequencyHz);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, saved.lfos[0].depth, loaded.lfos[0].depth);
    TEST_ASSERT_EQUAL_UINT8(saved.lfos[1].syncRatio, loaded.lfos[1].syncRatio);
    TEST_ASSERT_EQUAL_UINT8(saved.routes[0].channel, loaded.routes[0].channel);
    TEST_ASSERT_EQUAL_UINT8(saved.routes[0].ccMsb, loaded.routes[0].ccMsb);
    TEST_ASSERT_EQUAL_UINT8(saved.routes[1].target, loaded.routes[1].target);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, saved.routes[1].depth, loaded.routes[1].depth);
    TEST_ASSERT_EQUAL_UINT8(saved.slots[0].midiChannel, loaded.slots[0].midiChannel);
    TEST_ASSERT_EQUAL_UINT8(saved.slots[0].ef.mode, loaded.slots[0].ef.mode);
    TEST_ASSERT_EQUAL_UINT8(saved.slots[0].ef.gainTarget, loaded.slots[0].ef.gainTarget);
    TEST_ASSERT_EQUAL_UINT8(saved.slots[NUM_SLOTS - 1].midiChannel,
                            loaded.slots[NUM_SLOTS - 1].midiChannel);
    TEST_ASSERT_EQUAL_UINT16(saved.slots[NUM_SLOTS - 1].ef.releaseMs,
                             loaded.slots[NUM_SLOTS - 1].ef.releaseMs);
}
