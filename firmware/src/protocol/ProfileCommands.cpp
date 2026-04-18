#include "protocol/ProfileCommands.h"

#include "ConfigManager.h"
#include "FirmwareState.h"
#include "Globals.h"
#include "Modes.h"

namespace {
// Push persisted pot channel/CC mappings back into the live managers after config/profile changes.
void syncPotentiometerMappingsFromConfig() {
    potChannels.clear();
    for (uint8_t i = 0; i < configManager.getNumPots(); ++i) {
        const uint8_t channel = constrain(configManager.getPotChannel(i), 1, 16);
        const uint8_t cc = constrain(configManager.getPotCCNumber(i), 0, 127);
        configManager.setPotChannel(i, channel);
        configManager.setPotCCNumber(i, cc);
        potentiometerManager.setChannel(i, channel);
        potentiometerManager.setCCNumber(i, cc);
        potChannels.push_back(channel);
    }
}

// Build the baseline profile used when a slot is empty or gets reset.
ProfileData defaultProfileSnapshot() {
    ProfileData profile{};
    for (uint8_t i = 0; i < NUM_SLOTS; ++i) {
        profile.slots[i].midiChannel = 1;
    }
    return profile;
}
} // namespace

// Persist the current live deck state into one of the four firmware profile slots.
bool saveCurrentProfileSlot(uint8_t id) {
    if (id >= NUM_PROFILES) {
        return false;
    }
    g_activeProfile = id;
    configManager.setActiveProfile(id);
    configManager.saveProfile(id);
    configManager.saveEnvelopeSettings(potToEnvelopeMap, envelopeFollowers);
    return configManager.saveProfileSettings(id, captureProfileSnapshot());
}

// Load one profile slot into the live runtime, falling back to a baseline if the slot is blank.
bool loadProfileSlot(uint8_t id) {
    if (id >= NUM_PROFILES) {
        return false;
    }

    ProfileData profile{};
    const bool stored = configManager.loadProfileSettings(id, profile);
    g_activeProfile = id;
    configManager.setActiveProfile(id);

    if (stored) {
        configManager.loadProfile(id);
    } else {
        profile = defaultProfileSnapshot();
        for (uint8_t i = 0; i < configManager.getNumPots(); ++i) {
            configManager.setPotChannel(i, 1);
            configManager.setPotCCNumber(i, 0);
        }
        configManager.saveConfiguration();
    }

    syncPotentiometerMappingsFromConfig();
    applyProfileSnapshot(profile, true);
    refreshEfVoicesFromConfig();

    if (!stored) {
        configManager.saveProfile(id);
        configManager.saveProfileSettings(id, profile);
    }
    return true;
}

// Reset a profile slot to the baseline state and immediately apply that baseline live.
bool resetProfileSlot(uint8_t id) {
    if (id >= NUM_PROFILES) {
        return false;
    }

    const ProfileData profile = defaultProfileSnapshot();
    g_activeProfile = id;
    configManager.setActiveProfile(id);
    for (uint8_t i = 0; i < configManager.getNumPots(); ++i) {
        configManager.setPotChannel(i, 1);
        configManager.setPotCCNumber(i, 0);
    }
    configManager.saveConfiguration();
    syncPotentiometerMappingsFromConfig();
    applyProfileSnapshot(profile, true);
    refreshEfVoicesFromConfig();
    configManager.saveProfile(id);
    return configManager.saveProfileSettings(id, profile);
}
