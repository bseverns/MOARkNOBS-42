// Profile persistence helpers extracted from ConfigManager.
//
// Free functions for sanitizing ProfileData payloads and computing
// their CRC-16 checksums.  Called by ConfigManager::loadProfileSettings
// and ConfigManager::saveProfileSettings.

#ifndef PROFILE_STORAGE_H
#define PROFILE_STORAGE_H

#include "ProfileTypes.h"

/// Sanitize the EF subset within a profile slot.
ProfileEfSettings sanitizeProfileEfSettings(const ProfileEfSettings &settings);

/// Sanitize an entire profile payload so corrupt EEPROM data cannot
/// destabilize runtime.  Every field is clamped to valid ranges.
ProfileData sanitizeProfileData(const ProfileData &profile);

/// Compute the CRC-16 checksum over the profile payload bytes
/// following the crc field.
uint16_t computeProfileCrc(const ProfileData &profile);

#endif // PROFILE_STORAGE_H
