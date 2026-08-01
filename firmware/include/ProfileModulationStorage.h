#ifndef PROFILE_MODULATION_STORAGE_H
#define PROFILE_MODULATION_STORAGE_H

#include "ProfileModulationTypes.h"

uint16_t packProfileSlotArg(const SlotARGConfig &arg);
SlotARGConfig unpackProfileSlotArg(uint16_t packed);
ProfileModulationExtension sanitizeProfileModulation(
    const ProfileModulationExtension &extension);
uint16_t computeProfileModulationCrc(const ProfileModulationExtension &extension);

#endif // PROFILE_MODULATION_STORAGE_H
