#include "ProfileMigration.h"

#include "Arpeggiator.h"
#include "ProfileStorage.h"

#include <cstddef>

namespace {
constexpr uint16_t kProfileSettingsVersionV1 = 0x0001;
constexpr uint16_t kProfileSettingsVersionV2 = 0x0002;
constexpr uint16_t kProfileSettingsVersionV3 = 0x0003;
constexpr uint16_t kProfileSettingsVersionV4 = 0x0004;
constexpr uint16_t kProfileSettingsVersionV5 = 0x0005;
constexpr uint16_t kProfileSettingsVersionV6 = 0x0006;

struct __attribute__((packed)) LegacyProfileArpSettings {
    uint8_t lengthTicks = 12;
    uint8_t shape = 0;
    uint8_t swingPercent = 0;
    uint8_t gatePercent = 50;
    uint8_t octaveRange = 0;
};

struct __attribute__((packed)) LegacyProfileArpSettingsV6 {
    uint8_t lengthTicks = 12;
    uint8_t shape = 0;
    uint8_t swingPercent = 0;
    uint8_t gatePercent = 50;
    uint8_t octaveRange = 0;
    uint8_t patternLength = Arpeggiator::DEFAULT_PATTERN_LENGTH;
};

struct __attribute__((packed)) LegacyProfileEfSettings {
    uint8_t mode = 0;
    uint8_t autoBaseline = 1;
    uint8_t autoGain = 1;
    uint8_t gateThreshold = 16;
    uint8_t gateHysteresis = 4;
    uint8_t activityThreshold = 4;
    uint8_t gainTarget = 102;
    uint16_t attackMs = 5;
    uint16_t releaseMs = 20;
    uint16_t rmsWindowMs = 50;
    uint16_t baselineTauMs = 2000;
    uint16_t gainTauMs = 3000;
};

struct __attribute__((packed)) LegacyProfileLfoRoute {
    uint8_t type = 0;
    uint8_t lfoIndex = 0;
    float depth = 1.0f;
    uint8_t target = 0;
    uint8_t channel = 1;
    uint8_t ccMsb = 0;
    uint8_t ccLsb = 32;
};

struct __attribute__((packed)) LegacyProfileSlotSettings {
    uint8_t midiChannel = 1;
    LegacyProfileEfSettings ef{};
};

struct __attribute__((packed)) LegacyProfileSlotSettingsV2 {
    uint8_t midiChannel = 1;
    int8_t followerIndex = -1;
    LegacyProfileEfSettings ef{};
};

struct __attribute__((packed)) LegacyProfileDataV1 {
    uint16_t version = kProfileSettingsVersionV1;
    uint16_t crc = 0;
    uint8_t routeCount = 0;
    LegacyProfileArpSettings arp{};
    ProfileLedSettings led{};
    ProfileLfoSettings lfos[PROFILE_LFO_COUNT]{};
    LegacyProfileLfoRoute routes[PROFILE_MAX_ROUTES]{};
    LegacyProfileSlotSettings slots[NUM_SLOTS]{};
};

struct __attribute__((packed)) LegacyProfileDataV2 {
    uint16_t version = kProfileSettingsVersionV2;
    uint16_t crc = 0;
    uint8_t routeCount = 0;
    LegacyProfileArpSettings arp{};
    ProfileLedSettings led{};
    ProfileLfoSettings lfos[PROFILE_LFO_COUNT]{};
    LegacyProfileLfoRoute routes[PROFILE_MAX_ROUTES]{};
    LegacyProfileSlotSettingsV2 slots[NUM_SLOTS]{};
};

struct __attribute__((packed)) LegacyProfileDataV3 {
    uint16_t version = kProfileSettingsVersionV3;
    uint16_t crc = 0;
    uint8_t routeCount = 0;
    LegacyProfileArpSettings arp{};
    ProfileLedSettings led{};
    ProfileLfoSettings lfos[PROFILE_LFO_COUNT]{};
    ProfileLfoRoute routes[PROFILE_MAX_ROUTES]{};
    LegacyProfileSlotSettingsV2 slots[NUM_SLOTS]{};
};

struct __attribute__((packed)) LegacyProfileDataV4 {
    uint16_t version = kProfileSettingsVersionV4;
    uint16_t crc = 0;
    uint8_t routeCount = 0;
    LegacyProfileArpSettings arp{};
    ProfileLedSettings led{};
    ProfileLfoSettings lfos[PROFILE_LFO_COUNT]{};
    ProfileLfoRoute routes[PROFILE_MAX_ROUTES]{};
    ProfileSlotSettings slots[NUM_SLOTS]{};
};

struct __attribute__((packed)) LegacyProfileDataV5 {
    uint16_t version = kProfileSettingsVersionV5;
    uint16_t crc = 0;
    uint8_t routeCount = 0;
    LegacyProfileArpSettings arp{};
    ProfileLedSettings led{};
    ProfileClockSettings clock{};
    ProfileNoteDynamicsSettings noteDynamics{};
    ProfileJitterSettings jitter{};
    ProfileLfoSettings lfos[PROFILE_LFO_COUNT]{};
    ProfileLfoRoute routes[PROFILE_MAX_ROUTES]{};
    ProfileSlotSettings slots[NUM_SLOTS]{};
};

struct __attribute__((packed)) LegacyProfileDataV6 {
    uint16_t version = kProfileSettingsVersionV6;
    uint16_t crc = 0;
    uint8_t routeCount = 0;
    LegacyProfileArpSettingsV6 arp{};
    ProfileLedSettings led{};
    ProfileClockSettings clock{};
    ProfileNoteDynamicsSettings noteDynamics{};
    ProfileJitterSettings jitter{};
    ProfileLfoSettings lfos[PROFILE_LFO_COUNT]{};
    ProfileLfoRoute routes[PROFILE_MAX_ROUTES]{};
    ProfileSlotSettings slots[NUM_SLOTS]{};
};

template <typename T> void readRecord(const StorageBackend &storage, uint16_t base, T &value) {
    storage.readBytes(base, &value, sizeof(T));
}

uint16_t updateCrc(uint16_t crc, uint8_t data) {
    crc ^= data;
    for (uint8_t bit = 0; bit < 8; ++bit) {
        crc = (crc & 1) ? static_cast<uint16_t>((crc >> 1) ^ 0xA001) : crc >> 1;
    }
    return crc;
}

template <typename T> uint16_t computeLegacyProfileCrc(const T &profile) {
    const uint8_t *bytes = reinterpret_cast<const uint8_t *>(&profile);
    uint16_t crc = 0xFFFF;
    for (size_t i = offsetof(T, routeCount); i < sizeof(T); ++i) {
        crc = updateCrc(crc, bytes[i]);
    }
    return crc;
}

template <typename T, size_t N> void copyFixedArray(T (&dest)[N], const T (&source)[N]) {
    for (size_t i = 0; i < N; ++i) dest[i] = source[i];
}

void assignAllLegacyArpSlots(ProfileArpSettings &arp) {
    for (uint8_t slot = 0; slot < NUM_SLOTS; ++slot) {
        arp.assignedSlots[slot / 8U] |= static_cast<uint8_t>(1U << (slot % 8U));
    }
}

ProfileArpSettings migrateLegacyArp(const LegacyProfileArpSettings &legacy) {
    ProfileArpSettings arp{};
    arp.lengthTicks = legacy.lengthTicks;
    arp.shape = legacy.shape;
    arp.swingPercent = legacy.swingPercent;
    arp.gatePercent = legacy.gatePercent;
    arp.octaveRange = legacy.octaveRange;
    arp.patternLength = Arpeggiator::DEFAULT_PATTERN_LENGTH;
    assignAllLegacyArpSlots(arp);
    return arp;
}

ProfileArpSettings migrateLegacyArp(const LegacyProfileArpSettingsV6 &legacy) {
    ProfileArpSettings arp{};
    arp.lengthTicks = legacy.lengthTicks;
    arp.shape = legacy.shape;
    arp.swingPercent = legacy.swingPercent;
    arp.gatePercent = legacy.gatePercent;
    arp.octaveRange = legacy.octaveRange;
    arp.patternLength = legacy.patternLength;
    assignAllLegacyArpSlots(arp);
    return arp;
}

ProfileEfSettings migrateLegacyProfileEf(const LegacyProfileEfSettings &legacy) {
    ProfileEfSettings ef{};
    ef.mode = legacy.mode;
    ef.autoBaseline = legacy.autoBaseline;
    ef.autoGain = legacy.autoGain;
    ef.gateThreshold = legacy.gateThreshold;
    ef.gateHysteresis = legacy.gateHysteresis;
    ef.activityThreshold = legacy.activityThreshold;
    ef.gainTarget = legacy.gainTarget;
    ef.destinationMode = static_cast<uint8_t>(EfDestinationMode::AddClamp);
    ef.attackMs = legacy.attackMs;
    ef.releaseMs = legacy.releaseMs;
    ef.rmsWindowMs = legacy.rmsWindowMs;
    ef.baselineTauMs = legacy.baselineTauMs;
    ef.gainTauMs = legacy.gainTauMs;
    return ef;
}

ProfileLfoRoute migrateLegacyRoute(const LegacyProfileLfoRoute &legacy) {
    ProfileLfoRoute route{};
    route.type = legacy.type;
    route.lfoIndex = legacy.lfoIndex;
    route.depth = legacy.depth;
    route.target = legacy.target;
    route.channel = legacy.channel;
    route.ccMsb = legacy.ccMsb;
    route.ccLsb = legacy.ccLsb;
    return route;
}

ProfileDecodeResult finishDecode(ProfileData &destination, const ProfileData &decoded) {
    destination = sanitizeProfileData(decoded);
    destination.crc = computeProfileCrc(destination);
    return ProfileDecodeResult::Success;
}

template <typename T>
bool readLegacyRecord(const StorageBackend &storage, uint16_t base, T &legacy) {
    if (!storage.contains(base, sizeof(T))) return false;
    readRecord(storage, base, legacy);
    return legacy.crc == computeLegacyProfileCrc(legacy);
}
} // namespace

ProfileDecodeResult decodeStoredProfile(
    const StorageBackend &storage, uint16_t base,
    const std::array<MIDISlot, NUM_SLOTS> &liveSlots, ProfileData &profile) {
    if (!storage.contains(base, sizeof(ProfileData))) {
        return ProfileDecodeResult::InsufficientStorage;
    }

    uint16_t version = 0;
    readRecord(storage, base, version);
    if (version == PROFILE_SETTINGS_VERSION) {
        ProfileData stored{};
        readRecord(storage, base, stored);
        if (stored.crc != computeProfileCrc(stored)) {
            return ProfileDecodeResult::ChecksumMismatch;
        }
        return finishDecode(profile, stored);
    }

    ProfileData migrated{};
    migrated.version = PROFILE_SETTINGS_VERSION;
    if (version == kProfileSettingsVersionV6) {
        LegacyProfileDataV6 legacy{};
        if (!readLegacyRecord(storage, base, legacy)) {
            return ProfileDecodeResult::ChecksumMismatch;
        }
        migrated.routeCount = legacy.routeCount;
        migrated.arp = migrateLegacyArp(legacy.arp);
        migrated.led = legacy.led;
        migrated.clock = legacy.clock;
        migrated.noteDynamics = legacy.noteDynamics;
        migrated.jitter = legacy.jitter;
        copyFixedArray(migrated.lfos, legacy.lfos);
        copyFixedArray(migrated.routes, legacy.routes);
        copyFixedArray(migrated.slots, legacy.slots);
        return finishDecode(profile, migrated);
    }
    if (version == kProfileSettingsVersionV5) {
        LegacyProfileDataV5 legacy{};
        if (!readLegacyRecord(storage, base, legacy)) {
            return ProfileDecodeResult::ChecksumMismatch;
        }
        migrated.routeCount = legacy.routeCount;
        migrated.arp = migrateLegacyArp(legacy.arp);
        migrated.led = legacy.led;
        migrated.clock = legacy.clock;
        migrated.noteDynamics = legacy.noteDynamics;
        migrated.jitter = legacy.jitter;
        copyFixedArray(migrated.lfos, legacy.lfos);
        copyFixedArray(migrated.routes, legacy.routes);
        copyFixedArray(migrated.slots, legacy.slots);
        return finishDecode(profile, migrated);
    }
    if (version == kProfileSettingsVersionV4) {
        LegacyProfileDataV4 legacy{};
        if (!readLegacyRecord(storage, base, legacy)) {
            return ProfileDecodeResult::ChecksumMismatch;
        }
        migrated.routeCount = legacy.routeCount;
        migrated.arp = migrateLegacyArp(legacy.arp);
        migrated.led = legacy.led;
        copyFixedArray(migrated.lfos, legacy.lfos);
        copyFixedArray(migrated.routes, legacy.routes);
        copyFixedArray(migrated.slots, legacy.slots);
        return finishDecode(profile, migrated);
    }
    if (version == kProfileSettingsVersionV3) {
        LegacyProfileDataV3 legacy{};
        if (!readLegacyRecord(storage, base, legacy)) {
            return ProfileDecodeResult::ChecksumMismatch;
        }
        migrated.routeCount = legacy.routeCount;
        migrated.arp = migrateLegacyArp(legacy.arp);
        migrated.led = legacy.led;
        copyFixedArray(migrated.lfos, legacy.lfos);
        copyFixedArray(migrated.routes, legacy.routes);
        for (uint8_t i = 0; i < NUM_SLOTS; ++i) {
            migrated.slots[i].midiChannel = legacy.slots[i].midiChannel;
            migrated.slots[i].followerIndex = legacy.slots[i].followerIndex;
            migrated.slots[i].ef = migrateLegacyProfileEf(legacy.slots[i].ef);
        }
        return finishDecode(profile, migrated);
    }
    if (version == kProfileSettingsVersionV2) {
        LegacyProfileDataV2 legacy{};
        if (!readLegacyRecord(storage, base, legacy)) {
            return ProfileDecodeResult::ChecksumMismatch;
        }
        migrated.routeCount = legacy.routeCount;
        migrated.arp = migrateLegacyArp(legacy.arp);
        migrated.led = legacy.led;
        copyFixedArray(migrated.lfos, legacy.lfos);
        for (uint8_t i = 0; i < PROFILE_MAX_ROUTES; ++i) {
            migrated.routes[i] = migrateLegacyRoute(legacy.routes[i]);
        }
        for (uint8_t i = 0; i < NUM_SLOTS; ++i) {
            migrated.slots[i].midiChannel = legacy.slots[i].midiChannel;
            migrated.slots[i].followerIndex = legacy.slots[i].followerIndex;
            migrated.slots[i].ef = migrateLegacyProfileEf(legacy.slots[i].ef);
        }
        return finishDecode(profile, migrated);
    }
    if (version == kProfileSettingsVersionV1) {
        LegacyProfileDataV1 legacy{};
        if (!readLegacyRecord(storage, base, legacy)) {
            return ProfileDecodeResult::ChecksumMismatch;
        }
        migrated.routeCount = legacy.routeCount;
        migrated.arp = migrateLegacyArp(legacy.arp);
        migrated.led = legacy.led;
        copyFixedArray(migrated.lfos, legacy.lfos);
        for (uint8_t i = 0; i < PROFILE_MAX_ROUTES; ++i) {
            migrated.routes[i] = migrateLegacyRoute(legacy.routes[i]);
        }
        for (uint8_t i = 0; i < NUM_SLOTS; ++i) {
            migrated.slots[i].midiChannel = legacy.slots[i].midiChannel;
            migrated.slots[i].followerIndex = liveSlots[i].getEnvelopeFollowerIndex();
            migrated.slots[i].ef = migrateLegacyProfileEf(legacy.slots[i].ef);
        }
        return finishDecode(profile, migrated);
    }

    return ProfileDecodeResult::UnsupportedVersion;
}
