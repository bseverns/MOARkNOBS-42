#include "protocol/ProfileCommands.h"

#include "ConfigManager.h"
#include "FirmwareState.h"
#include "Globals.h"
#include "Modes.h"
#include "protocol/SceneStorage.h"

// ProfileCommands.cpp is the lifecycle layer for stored profile slots.
//
// Reading order:
// 1. local helpers for syncing pot/channel mappings and building a default profile
// 2. save current live state into one profile slot
// 3. load one slot back into live runtime state
// 4. reset one slot to the baseline profile

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

// Every profile transition begins by declaring which slot the firmware now considers "active."
void selectActiveProfileSlot(uint8_t id) {
    g_activeProfile = id;
    configManager.setActiveProfile(id);
}

// A blank/reset profile returns pots to the simplest safe baseline: channel 1, CC 0.
void applyBaselinePotMappings() {
    for (uint8_t i = 0; i < configManager.getNumPots(); ++i) {
        configManager.setPotChannel(i, 1);
        configManager.setPotCCNumber(i, 0);
    }
    configManager.saveConfiguration();
}

// After profile/config state changes, rebuild the live runtime so the stored snapshot becomes
// audible.
void rebuildRuntimeFromProfile(const ProfileData &profile,
                               const ProfileModulationExtension &modulation) {
    syncPotentiometerMappingsFromConfig();
    applyCompleteProfile(profile, modulation, true);
    refreshEfVoicesFromConfig();
}

// When a slot was blank, we immediately persist the baseline so future loads see a concrete
// profile.
void persistMaterializedProfileSlot(uint8_t id, const ProfileData &profile,
                                    const ProfileModulationExtension &modulation) {
    StorageBackend *storage = ConfigManager::getStorageBackend();
    if (storage->supportsTransactions() && !storage->beginTransaction()) {
        return;
    }
    configManager.saveProfile(id);
    if (!configManager.saveProfileSettings(id, profile) ||
        !configManager.saveProfileModulation(id, modulation)) {
        storage->abortTransaction();
        return;
    }
    if (storage->supportsTransactions() && !storage->commitTransaction()) {
        storage->abortTransaction();
    }
}
} // namespace

// Persist the current live deck state into one of the four firmware profile slots.
bool saveCurrentProfileSlot(uint8_t id) {
    if (id >= NUM_PROFILES) {
        return false;
    }

    StorageBackend *storage = ConfigManager::getStorageBackend();
    if (storage->supportsTransactions() && !storage->beginTransaction()) {
        return false;
    }

    selectActiveProfileSlot(id);
    configManager.saveProfile(id);
    configManager.saveEnvelopeSettings(potToEnvelopeMap, envelopeFollowers);
    if (!configManager.saveProfileSettings(id, captureProfileSnapshot()) ||
        !configManager.saveProfileModulation(id, captureProfileModulation())) {
        storage->abortTransaction();
        return false;
    }
    if (!storage->supportsTransactions()) {
        return true;
    }
    const bool committed = storage->commitTransaction();
    if (!committed) storage->abortTransaction();
    return committed;
}

// Load one profile slot into the live runtime, falling back to a baseline if the slot is blank.
bool loadProfileSlot(uint8_t id) {
    if (id >= NUM_PROFILES) {
        return false;
    }

    ProfileData profile{};
    const bool stored = configManager.loadProfileSettings(id, profile);
    ProfileModulationExtension modulation{};
    const bool modulationStored = configManager.loadProfileModulation(id, modulation);

    if (stored) {
        // Extended profile state is not sufficient on its own: reject the
        // transition if neither legacy pot/CC mapping copy validates.
        if (!configManager.loadProfile(id)) return false;
        if (!modulationStored) modulation = captureProfileModulation();
    } else {
        profile = defaultProfileSnapshot();
        modulation = defaultProfileModulationSnapshot();
        applyBaselinePotMappings();
    }

    selectActiveProfileSlot(id);
    rebuildRuntimeFromProfile(profile, modulation);

    if (!stored || !modulationStored) {
        persistMaterializedProfileSlot(id, profile, modulation);
    }
    return true;
}

// Reset a profile slot to the baseline state and immediately apply that baseline live.
bool resetProfileSlot(uint8_t id) {
    if (id >= NUM_PROFILES) {
        return false;
    }

    const ProfileData profile = defaultProfileSnapshot();
    const ProfileModulationExtension modulation = defaultProfileModulationSnapshot();
    const SceneStorage::ConfigState prior = SceneStorage::captureConfigState();
    StorageBackend *storage = ConfigManager::getStorageBackend();
    if (storage->supportsTransactions() && !storage->beginTransaction()) {
        return false;
    }
    selectActiveProfileSlot(id);
    applyBaselinePotMappings();
    rebuildRuntimeFromProfile(profile, modulation);
    configManager.saveProfile(id);
    if (!configManager.saveProfileSettings(id, profile) ||
        !configManager.saveProfileModulation(id, modulation)) {
        storage->abortTransaction();
        SceneStorage::applyConfigState(prior, false);
        return false;
    }
    if (!storage->supportsTransactions()) return true;
    if (storage->commitTransaction()) return true;
    storage->abortTransaction();
    SceneStorage::applyConfigState(prior, false);
    return false;
}
