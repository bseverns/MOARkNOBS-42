#ifndef PROFILE_MODULATION_TYPES_H
#define PROFILE_MODULATION_TYPES_H

#include "MIDITypes.h"
#include "StorageLayout.h"

#include <cstdint>

inline constexpr uint8_t PROFILE_LFO_COUNT = 2;
inline constexpr uint16_t PROFILE_MODULATION_VERSION = 0x0001;

struct __attribute__((packed)) ProfileSlotModSettings {
    uint16_t argPacked = 0;
    SlotLfoLane lfo[PROFILE_LFO_COUNT]{};
};

struct __attribute__((packed)) ProfileModulationExtension {
    uint16_t version = PROFILE_MODULATION_VERSION;
    uint16_t crc = 0;
    ProfileSlotModSettings slots[NUM_SLOTS]{};
};

static_assert(sizeof(ProfileSlotModSettings) == 6,
              "Profile slot modulation payload must stay compact");
static_assert(sizeof(ProfileModulationExtension) <= EEPROM_PROFILE_MODULATION_BLOCK_SIZE,
              "Profile modulation extension exceeds its storage block");

#endif // PROFILE_MODULATION_TYPES_H
