#include "unity_config.h"
#include <unity.h>
#include <cmath>

#include "BoardPowerProfile.h"
#include "Arpeggiator.h"
#include "ConfigManager.h"
#include "EnvelopeFollower.h"
#include <EEPROM.h>
#include <cstddef>

namespace {
struct __attribute__((packed)) LegacyProfileArpSettingsV5 {
    uint8_t lengthTicks = 12;
    uint8_t shape = 0;
    uint8_t swingPercent = 0;
    uint8_t gatePercent = 50;
    uint8_t octaveRange = 0;
};

struct __attribute__((packed)) LegacyProfileDataV5Fixture {
    uint16_t version = 0x0005;
    uint16_t crc = 0;
    uint8_t routeCount = 0;
    LegacyProfileArpSettingsV5 arp{};
    ProfileLedSettings led{};
    ProfileClockSettings clock{};
    ProfileNoteDynamicsSettings noteDynamics{};
    ProfileJitterSettings jitter{};
    ProfileLfoSettings lfos[PROFILE_LFO_COUNT]{};
    ProfileLfoRoute routes[PROFILE_MAX_ROUTES]{};
    ProfileSlotSettings slots[NUM_SLOTS]{};
};

struct __attribute__((packed)) LegacyProfileArpSettingsV6Fixture {
    uint8_t lengthTicks = 12;
    uint8_t shape = 0;
    uint8_t swingPercent = 0;
    uint8_t gatePercent = 50;
    uint8_t octaveRange = 0;
    uint8_t patternLength = Arpeggiator::DEFAULT_PATTERN_LENGTH;
};

struct __attribute__((packed)) LegacyProfileDataV6Fixture {
    uint16_t version = 0x0006;
    uint16_t crc = 0;
    uint8_t routeCount = 0;
    LegacyProfileArpSettingsV6Fixture arp{};
    ProfileLedSettings led{};
    ProfileClockSettings clock{};
    ProfileNoteDynamicsSettings noteDynamics{};
    ProfileJitterSettings jitter{};
    ProfileLfoSettings lfos[PROFILE_LFO_COUNT]{};
    ProfileLfoRoute routes[PROFILE_MAX_ROUTES]{};
    ProfileSlotSettings slots[NUM_SLOTS]{};
};

uint16_t legacyV5Crc(const LegacyProfileDataV5Fixture &profile) {
    const uint8_t *bytes = reinterpret_cast<const uint8_t *>(&profile);
    uint16_t crc = 0xFFFF;
    for (size_t i = offsetof(LegacyProfileDataV5Fixture, routeCount); i < sizeof(profile); ++i) {
        crc ^= bytes[i];
        for (uint8_t bit = 0; bit < 8; ++bit) {
            crc = (crc & 1) ? static_cast<uint16_t>((crc >> 1) ^ 0xA001) : crc >> 1;
        }
    }
    return crc;
}

void writeLegacyV5Profile(uint8_t id, const LegacyProfileDataV5Fixture &profile) {
    const uint16_t base = EEPROM_PROFILE_SETTINGS_START(id);
    const uint8_t *bytes = reinterpret_cast<const uint8_t *>(&profile);
    for (size_t i = 0; i < sizeof(profile); ++i) {
        EEPROM.update(static_cast<int>(base + i), bytes[i]);
    }
}

uint16_t legacyV6Crc(const LegacyProfileDataV6Fixture &profile) {
    const uint8_t *bytes = reinterpret_cast<const uint8_t *>(&profile);
    uint16_t crc = 0xFFFF;
    for (size_t i = offsetof(LegacyProfileDataV6Fixture, routeCount); i < sizeof(profile); ++i) {
        crc ^= bytes[i];
        for (uint8_t bit = 0; bit < 8; ++bit) {
            crc = (crc & 1) ? static_cast<uint16_t>((crc >> 1) ^ 0xA001) : crc >> 1;
        }
    }
    return crc;
}

void writeLegacyV6Profile(uint8_t id, const LegacyProfileDataV6Fixture &profile) {
    const uint16_t base = EEPROM_PROFILE_SETTINGS_START(id);
    const uint8_t *bytes = reinterpret_cast<const uint8_t *>(&profile);
    for (size_t i = 0; i < sizeof(profile); ++i) {
        EEPROM.update(static_cast<int>(base + i), bytes[i]);
    }
}

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
    profile.arp.patternLength = 11;
    profile.arp.assignedSlots[0] = static_cast<uint8_t>(1U << 2U);
    profile.arp.assignedSlots[2] = static_cast<uint8_t>(1U << 1U);
    profile.led.brightness = BoardPowerProfile::kLedBrightnessCap;
    profile.led.r = 12;
    profile.led.g = 34;
    profile.led.b = 56;
    profile.clock.tappedBpm = 97.5f;
    profile.clock.clockOutEnabled = 1;
    profile.clock.followExternalClock = 0;
    profile.noteDynamics.velocityShift = -11;
    profile.noteDynamics.changeProbability = 72;
    profile.jitter.depth = 0.33f;
    profile.jitter.smoothness = 0.81f;
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
    profile.routes[0].amount = -75;
    profile.routes[0].minValue = 12;
    profile.routes[0].maxValue = 96;
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
        profile.slots[i].ef.destinationMode =
            static_cast<uint8_t>(i % (static_cast<uint8_t>(EfDestinationMode::Centered) + 1));
        profile.slots[i].ef.attackMs = static_cast<uint16_t>(10 + i);
        profile.slots[i].ef.releaseMs = static_cast<uint16_t>(30 + i);
    }
    return profile;
}
} // namespace

void test_profile_crc_rejects_corruption() {
    // Corrupt a byte after saving and verify the CRC check fails without
    // replacing a snapshot the caller may still need as its live fallback.
    ConfigManager cfg(NUM_POTS, NUM_BUTTONS);
    clearProfileSettingsBlock(0);

    ProfileData profile{};
    profile.routeCount = 1;
    profile.routes[0].type = 1;
    TEST_ASSERT_TRUE(cfg.saveProfileSettings(0, profile));

    const uint16_t base = EEPROM_PROFILE_SETTINGS_START(0);
    uint8_t byte = EEPROM.read(base + sizeof(uint16_t));
    EEPROM.update(base + sizeof(uint16_t), static_cast<uint8_t>(byte ^ 0xFF));

    ProfileData loaded = makePopulatedProfile();
    const ProfileData retained = loaded;
    TEST_ASSERT_FALSE(cfg.loadProfileSettings(0, loaded));
    TEST_ASSERT_EQUAL_MEMORY(&retained, &loaded, sizeof(ProfileData));
}

void test_profile_bounds_clamp() {
    // Out-of-range fields should be sanitized on load.
    ConfigManager cfg(NUM_POTS, NUM_BUTTONS);
    clearProfileSettingsBlock(1);

    ProfileData profile{};
    profile.routeCount = static_cast<uint8_t>(PROFILE_MAX_ROUTES + 3);
    profile.clock.tappedBpm = NAN;
    profile.clock.clockOutEnabled = 9;
    profile.clock.followExternalClock = 3;
    profile.noteDynamics.velocityShift = 127;
    profile.noteDynamics.changeProbability = 255;
    profile.jitter.depth = INFINITY;
    profile.jitter.smoothness = -1.0f;
    for (uint8_t i = 0; i < NUM_SLOTS; ++i) {
        profile.slots[i].midiChannel = 0;
        profile.slots[i].ef.mode = 99;
        profile.slots[i].ef.destinationMode = 99;
        profile.slots[i].ef.attackMs = 0;
        profile.slots[i].ef.releaseMs = 65535;
    }
    TEST_ASSERT_TRUE(cfg.saveProfileSettings(1, profile));

    ProfileData loaded{};
    TEST_ASSERT_TRUE(cfg.loadProfileSettings(1, loaded));
    TEST_ASSERT_TRUE(loaded.routeCount <= PROFILE_MAX_ROUTES);
    TEST_ASSERT_TRUE(loaded.led.brightness <= BoardPowerProfile::kLedBrightnessCap);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 120.0f, loaded.clock.tappedBpm);
    TEST_ASSERT_EQUAL_UINT8(1, loaded.clock.clockOutEnabled);
    TEST_ASSERT_EQUAL_UINT8(1, loaded.clock.followExternalClock);
    TEST_ASSERT_EQUAL_INT8(63, loaded.noteDynamics.velocityShift);
    TEST_ASSERT_EQUAL_UINT8(100, loaded.noteDynamics.changeProbability);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, loaded.jitter.depth);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, loaded.jitter.smoothness);
    for (uint8_t i = 0; i < NUM_SLOTS; ++i) {
        TEST_ASSERT_TRUE(loaded.slots[i].midiChannel >= 1);
        TEST_ASSERT_TRUE(loaded.slots[i].midiChannel <= 16);
        TEST_ASSERT_TRUE(loaded.slots[i].ef.mode <=
                         static_cast<uint8_t>(EnvelopeFollower::EFMode::Follower));
        TEST_ASSERT_TRUE(loaded.slots[i].ef.destinationMode <=
                         static_cast<uint8_t>(EfDestinationMode::Centered));
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
    TEST_ASSERT_EQUAL_UINT8(saved.arp.patternLength, loaded.arp.patternLength);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(saved.arp.assignedSlots, loaded.arp.assignedSlots,
                                  Arpeggiator::ASSIGNMENT_BYTES);
    TEST_ASSERT_EQUAL_UINT8(saved.led.brightness, loaded.led.brightness);
    TEST_ASSERT_EQUAL_UINT8(saved.led.r, loaded.led.r);
    TEST_ASSERT_EQUAL_UINT8(saved.led.g, loaded.led.g);
    TEST_ASSERT_EQUAL_UINT8(saved.led.b, loaded.led.b);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, saved.clock.tappedBpm, loaded.clock.tappedBpm);
    TEST_ASSERT_EQUAL_UINT8(saved.clock.clockOutEnabled, loaded.clock.clockOutEnabled);
    TEST_ASSERT_EQUAL_UINT8(saved.clock.followExternalClock, loaded.clock.followExternalClock);
    TEST_ASSERT_EQUAL_INT8(saved.noteDynamics.velocityShift, loaded.noteDynamics.velocityShift);
    TEST_ASSERT_EQUAL_UINT8(saved.noteDynamics.changeProbability,
                            loaded.noteDynamics.changeProbability);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, saved.jitter.depth, loaded.jitter.depth);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, saved.jitter.smoothness, loaded.jitter.smoothness);
    TEST_ASSERT_EQUAL_UINT8(saved.lfos[0].shape, loaded.lfos[0].shape);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, saved.lfos[0].frequencyHz, loaded.lfos[0].frequencyHz);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, saved.lfos[0].depth, loaded.lfos[0].depth);
    TEST_ASSERT_EQUAL_UINT8(saved.lfos[1].syncRatio, loaded.lfos[1].syncRatio);
    TEST_ASSERT_EQUAL_UINT8(saved.routes[0].channel, loaded.routes[0].channel);
    TEST_ASSERT_EQUAL_UINT8(saved.routes[0].ccMsb, loaded.routes[0].ccMsb);
    TEST_ASSERT_EQUAL_INT8(saved.routes[0].amount, loaded.routes[0].amount);
    TEST_ASSERT_EQUAL_UINT8(saved.routes[0].minValue, loaded.routes[0].minValue);
    TEST_ASSERT_EQUAL_UINT8(saved.routes[0].maxValue, loaded.routes[0].maxValue);
    TEST_ASSERT_EQUAL_UINT8(saved.routes[1].target, loaded.routes[1].target);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, saved.routes[1].depth, loaded.routes[1].depth);
    TEST_ASSERT_EQUAL_UINT8(saved.slots[0].midiChannel, loaded.slots[0].midiChannel);
    TEST_ASSERT_EQUAL_UINT8(saved.slots[0].ef.mode, loaded.slots[0].ef.mode);
    TEST_ASSERT_EQUAL_UINT8(saved.slots[0].ef.destinationMode, loaded.slots[0].ef.destinationMode);
    TEST_ASSERT_EQUAL_UINT8(saved.slots[0].ef.gainTarget, loaded.slots[0].ef.gainTarget);
    TEST_ASSERT_EQUAL_UINT8(saved.slots[NUM_SLOTS - 1].midiChannel,
                            loaded.slots[NUM_SLOTS - 1].midiChannel);
    TEST_ASSERT_EQUAL_UINT16(saved.slots[NUM_SLOTS - 1].ef.releaseMs,
                             loaded.slots[NUM_SLOTS - 1].ef.releaseMs);
}

void test_profile_v5_migration_preserves_state_and_defaults_pattern_length() {
    ConfigManager cfg(NUM_POTS, NUM_BUTTONS);
    clearProfileSettingsBlock(3);

    LegacyProfileDataV5Fixture legacy{};
    legacy.routeCount = 1;
    legacy.arp.lengthTicks = 19;
    legacy.arp.shape = 3;
    legacy.arp.swingPercent = 27;
    legacy.arp.gatePercent = 73;
    legacy.arp.octaveRange = 2;
    legacy.led.r = 41;
    legacy.clock.tappedBpm = 111.5f;
    legacy.noteDynamics.velocityShift = -9;
    legacy.jitter.depth = 0.37f;
    legacy.lfos[0].frequencyHz = 2.25f;
    legacy.routes[0].type = 1;
    legacy.routes[0].channel = 7;
    legacy.routes[0].ccMsb = 74;
    legacy.slots[0].midiChannel = 12;
    legacy.slots[0].followerIndex = 1;
    legacy.slots[0].ef.gateThreshold = 29;
    legacy.crc = legacyV5Crc(legacy);
    writeLegacyV5Profile(3, legacy);

    ProfileData loaded{};
    TEST_ASSERT_TRUE(cfg.loadProfileSettings(3, loaded));
    TEST_ASSERT_EQUAL_UINT16(PROFILE_SETTINGS_VERSION, loaded.version);
    TEST_ASSERT_EQUAL_UINT8(legacy.routeCount, loaded.routeCount);
    TEST_ASSERT_EQUAL_UINT8(legacy.arp.lengthTicks, loaded.arp.lengthTicks);
    TEST_ASSERT_EQUAL_UINT8(legacy.arp.shape, loaded.arp.shape);
    TEST_ASSERT_EQUAL_UINT8(legacy.arp.swingPercent, loaded.arp.swingPercent);
    TEST_ASSERT_EQUAL_UINT8(legacy.arp.gatePercent, loaded.arp.gatePercent);
    TEST_ASSERT_EQUAL_UINT8(legacy.arp.octaveRange, loaded.arp.octaveRange);
    TEST_ASSERT_EQUAL_UINT8(Arpeggiator::DEFAULT_PATTERN_LENGTH, loaded.arp.patternLength);
    for (uint8_t slot = 0; slot < NUM_SLOTS; ++slot) {
        TEST_ASSERT_NOT_EQUAL(0,
                              loaded.arp.assignedSlots[slot / 8U] & (1U << (slot % 8U)));
    }
    TEST_ASSERT_EQUAL_UINT8(legacy.led.r, loaded.led.r);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, legacy.clock.tappedBpm, loaded.clock.tappedBpm);
    TEST_ASSERT_EQUAL_INT8(legacy.noteDynamics.velocityShift, loaded.noteDynamics.velocityShift);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, legacy.jitter.depth, loaded.jitter.depth);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, legacy.lfos[0].frequencyHz, loaded.lfos[0].frequencyHz);
    TEST_ASSERT_EQUAL_UINT8(legacy.routes[0].ccMsb, loaded.routes[0].ccMsb);
    TEST_ASSERT_EQUAL_UINT8(legacy.slots[0].midiChannel, loaded.slots[0].midiChannel);
    TEST_ASSERT_EQUAL_INT8(legacy.slots[0].followerIndex, loaded.slots[0].followerIndex);
    TEST_ASSERT_EQUAL_UINT8(legacy.slots[0].ef.gateThreshold, loaded.slots[0].ef.gateThreshold);
}

void test_profile_v6_migration_preserves_pattern_and_explicitly_assigns_legacy_slots() {
    ConfigManager cfg(NUM_POTS, NUM_BUTTONS);
    clearProfileSettingsBlock(3);

    LegacyProfileDataV6Fixture legacy{};
    legacy.arp.lengthTicks = 18;
    legacy.arp.patternLength = 13;
    legacy.crc = legacyV6Crc(legacy);
    writeLegacyV6Profile(3, legacy);

    ProfileData loaded{};
    TEST_ASSERT_TRUE(cfg.loadProfileSettings(3, loaded));
    TEST_ASSERT_EQUAL_UINT16(PROFILE_SETTINGS_VERSION, loaded.version);
    TEST_ASSERT_EQUAL_UINT8(legacy.arp.lengthTicks, loaded.arp.lengthTicks);
    TEST_ASSERT_EQUAL_UINT8(legacy.arp.patternLength, loaded.arp.patternLength);
    for (uint8_t slot = 0; slot < NUM_SLOTS; ++slot) {
        TEST_ASSERT_NOT_EQUAL(0,
                              loaded.arp.assignedSlots[slot / 8U] & (1U << (slot % 8U)));
    }
}
