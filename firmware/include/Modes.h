#ifndef MODES_H
#define MODES_H

#include "ConfigManager.h"
#include "EnvelopeFollower.h"

// Modes.h is the "persisted musical state" header.
//
// This layer sits between EEPROM/profile storage and the live runtime objects.
// Read it as the answer to:
// - how do stored EF/profile values become live follower/LFO/LED/runtime state?
// - how do we capture the current live state back into a profile snapshot?

// Small translation helpers between stored config enums and live follower state.
MIDISlot::EfSettings::FilterType fromEnvelopeFilter(EnvelopeFollower::FilterType type);
void applyEfSettingsToFollower(EnvelopeFollower &ef, const MIDISlot::EfSettings &settings,
                               uint8_t followerIndex);

// Profile snapshot capture and restore.
ProfileData captureProfileSnapshot();
ProfileModulationExtension captureProfileModulation();
ProfileModulationExtension defaultProfileModulationSnapshot();
void applyProfileSnapshot(const ProfileData &profile, bool persistSlots);
void applyProfileModulation(const ProfileModulationExtension &extension, bool persistSlots);
void applyCompleteProfile(const ProfileData &profile,
                          const ProfileModulationExtension &extension,
                          bool persistSlots);
bool persistActiveProfileSnapshot();
void restoreActiveProfileRuntime(bool persistSnapshot);

// Boot/default reconstruction of modulation state.
void configureLFOs();
void refreshEfVoicesFromConfig();
bool initializeModes();

// Shared label/cache maintenance for UI and protocol readers.
void updateEnvelopeModeLabel(const char *label);

#endif // MODES_H
