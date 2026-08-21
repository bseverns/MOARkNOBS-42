#ifndef PROFILE_TYPES_H
#define PROFILE_TYPES_H

#include "MIDITypes.h"
#include "ProfileModulationTypes.h"
#include "StorageLayout.h"

#include <cstddef>
#include <cstdint>

inline constexpr uint8_t PROFILE_ARP_DEFAULT_PATTERN_LENGTH = 4;
inline constexpr size_t PROFILE_ARP_ASSIGNMENT_BYTES = (NUM_SLOTS + 7U) / 8U;

struct __attribute__((packed)) ProfileEfSettings {
    uint8_t mode = 0;
    uint8_t autoBaseline = 1;
    uint8_t autoGain = 1;
    uint8_t gateThreshold = 16;
    uint8_t gateHysteresis = 4;
    uint8_t activityThreshold = 4;
    uint8_t gainTarget = 102;
    uint8_t destinationMode = static_cast<uint8_t>(EfDestinationMode::AddClamp);
    uint16_t attackMs = 5;
    uint16_t releaseMs = 20;
    uint16_t rmsWindowMs = 50;
    uint16_t baselineTauMs = 2000;
    uint16_t gainTauMs = 3000;
};

struct __attribute__((packed)) ProfileSlotSettings {
    uint8_t midiChannel = 1;
    int8_t followerIndex = -1;
    ProfileEfSettings ef{};
};

struct __attribute__((packed)) ProfileArpSettings {
    uint8_t lengthTicks = 12;
    uint8_t shape = 0;
    uint8_t swingPercent = 0;
    uint8_t gatePercent = 50;
    uint8_t octaveRange = 0;
    uint8_t patternLength = PROFILE_ARP_DEFAULT_PATTERN_LENGTH;
    uint8_t assignedSlots[PROFILE_ARP_ASSIGNMENT_BYTES]{};
};

struct __attribute__((packed)) ProfileLedSettings {
    uint8_t brightness = 128;
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
};

struct __attribute__((packed)) ProfileClockSettings {
    float tappedBpm = 120.0f;
    uint8_t clockOutEnabled = 0;
    uint8_t followExternalClock = 1;
};

struct __attribute__((packed)) ProfileNoteDynamicsSettings {
    int8_t velocityShift = 0;
    uint8_t changeProbability = 100;
};

struct __attribute__((packed)) ProfileJitterSettings {
    float depth = 1.0f;
    float smoothness = 0.5f;
};

struct __attribute__((packed)) ProfileLfoSettings {
    uint8_t shape = 0;
    float frequencyHz = 1.0f;
    float depth = 0.0f;
    uint8_t bipolar = 1;
    uint8_t syncEnabled = 0;
    uint8_t syncRatio = 0;
};

struct __attribute__((packed)) ProfileLfoRoute {
    uint8_t type = 0;
    uint8_t lfoIndex = 0;
    float depth = 1.0f;
    uint8_t target = 0;
    uint8_t channel = 1;
    uint8_t ccMsb = 0;
    uint8_t ccLsb = 32;
    int8_t amount = 100;
    uint8_t minValue = 0;
    uint8_t maxValue = 127;
};

inline constexpr uint8_t PROFILE_MAX_ROUTES = 8;
inline constexpr uint16_t PROFILE_SETTINGS_VERSION = 0x0007;

struct __attribute__((packed)) ProfileData {
    uint16_t version = PROFILE_SETTINGS_VERSION;
    uint16_t crc = 0;
    uint8_t routeCount = 0;
    ProfileArpSettings arp{};
    ProfileLedSettings led{};
    ProfileClockSettings clock{};
    ProfileNoteDynamicsSettings noteDynamics{};
    ProfileJitterSettings jitter{};
    ProfileLfoSettings lfos[PROFILE_LFO_COUNT]{};
    ProfileLfoRoute routes[PROFILE_MAX_ROUTES]{};
    ProfileSlotSettings slots[NUM_SLOTS]{};
};

static_assert(sizeof(ProfileData) <= EEPROM_PROFILE_SETTINGS_BLOCK_SIZE,
              "ProfileData exceeds the allotted storage block size");

#endif // PROFILE_TYPES_H
