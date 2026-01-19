#ifndef MODES_H
#define MODES_H

#include "ConfigManager.h"
#include "EnvelopeFollower.h"

MIDISlot::EfSettings::FilterType fromEnvelopeFilter(EnvelopeFollower::FilterType type);
void applyEfSettingsToFollower(EnvelopeFollower &ef, const MIDISlot::EfSettings &settings,
                               uint8_t followerIndex);

ProfileData captureProfileSnapshot();
void applyProfileSnapshot(const ProfileData &profile, bool persistSlots);
void configureLFOs();
void refreshEfVoicesFromConfig();
void updateEnvelopeModeLabel(const char *label);
bool initializeModes();

#endif // MODES_H
