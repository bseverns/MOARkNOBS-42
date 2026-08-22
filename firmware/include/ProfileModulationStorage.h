#ifndef PROFILE_MODULATION_STORAGE_H
#define PROFILE_MODULATION_STORAGE_H

#include "ProfileModulationTypes.h"

#include <cstddef>
#include <cstdint>

uint16_t packProfileSlotArg(const SlotARGConfig &arg);
SlotARGConfig unpackProfileSlotArg(uint16_t packed);
ProfileModulationExtension sanitizeProfileModulation(
    const ProfileModulationExtension &extension);
uint16_t computeProfileModulationCrc(const ProfileModulationExtension &extension);
bool decodeProfileModulationV1(const uint8_t *bytes, size_t length,
                               ProfileModulationExtension &extension);

#endif // PROFILE_MODULATION_STORAGE_H
