#include "Modes.h"

#include <Arduino.h>
#include <algorithm>
#include <array>
#include <cstdint>

#include "FastLED.h"
#include "FirmwareState.h"
#include "Arpeggiator.h"
#include "LFO/LFOManager.h"
#include "LEDManager.h"
#include "EnvelopeFollower.h"
#include "ProfileModulationStorage.h"

// Modes.cpp is the bridge between persisted profile/config state and the live
// runtime objects that actually make sound, light, and movement.
//
// Reading order:
// 1. tiny enum/shape translation helpers
// 2. profile snapshot encode/decode helpers
// 3. profile capture/apply
// 4. default modulation-state reconstruction during boot
// 5. small shared label/cache maintenance

std::array<float, NUM_ENVELOPES> efBaseGains{};

// Translate the runtime follower enum back into the persisted profile enum so
// the config layer never depends on `EnvelopeFollower` internals.
MIDISlot::EfSettings::FilterType fromEnvelopeFilter(EnvelopeFollower::FilterType type) {
    switch (type) {
    case EnvelopeFollower::LOWPASS:
        return MIDISlot::EfSettings::FilterType::Lowpass;
    case EnvelopeFollower::HIGHPASS:
        return MIDISlot::EfSettings::FilterType::Highpass;
    case EnvelopeFollower::BANDPASS:
        return MIDISlot::EfSettings::FilterType::Bandpass;
    default:
        return MIDISlot::EfSettings::FilterType::Lowpass;
    }
}

// Push a slot's stored EF parameters back into a live follower and cache the
// baseline gain separately so LFO trim can modulate around the saved value.
void applyEfSettingsToFollower(EnvelopeFollower &ef, const MIDISlot::EfSettings &settings,
                               uint8_t followerIndex) {
    ef.configureFromEfSettings(settings);
    if (followerIndex < NUM_ENVELOPES) {
        efBaseGains[followerIndex] = settings.gain;
    }
}

namespace {
// Strip the runtime-only parts of `EfSettings` so profile storage keeps only
// the fields that should travel with a saved profile snapshot.
ProfileEfSettings profileEfFromSlot(const MIDISlot::EfSettings &settings) {
    ProfileEfSettings profile{};
    profile.mode = settings.efMode;
    profile.autoBaseline = settings.autoBaseline;
    profile.autoGain = settings.autoGain;
    profile.gateThreshold = settings.gateThreshold;
    profile.gateHysteresis = settings.gateHysteresis;
    profile.activityThreshold = settings.activityThreshold;
    profile.gainTarget = settings.gainTarget;
    profile.destinationMode = settings.destinationMode;
    profile.attackMs = settings.attackMs;
    profile.releaseMs = settings.releaseMs;
    profile.rmsWindowMs = settings.rmsWindowMs;
    profile.baselineTauMs = settings.baselineTauMs;
    profile.gainTauMs = settings.gainTauMs;
    return profile;
}

// Rehydrate the persisted profile subset back into the wider slot settings
// object before the rest of the firmware starts consuming it.
void applyProfileEfToSlot(const ProfileEfSettings &profile, MIDISlot::EfSettings &settings) {
    settings.efMode = profile.mode;
    settings.autoBaseline = profile.autoBaseline;
    settings.autoGain = profile.autoGain;
    settings.gateThreshold = profile.gateThreshold;
    settings.gateHysteresis = profile.gateHysteresis;
    settings.activityThreshold = profile.activityThreshold;
    settings.gainTarget = profile.gainTarget;
    settings.destinationMode = profile.destinationMode;
    settings.attackMs = profile.attackMs;
    settings.releaseMs = profile.releaseMs;
    settings.rmsWindowMs = profile.rmsWindowMs;
    settings.baselineTauMs = profile.baselineTauMs;
    settings.gainTauMs = profile.gainTauMs;
}
} // namespace

// 3a. Capture the subset of live runtime state that should travel with profile slots (A-D).
ProfileData captureProfileSnapshot() {
    ProfileData profile{};
    profile.routeCount = 0;

    profile.arp.lengthTicks = arpeggiator.getLength();
    profile.arp.shape = static_cast<uint8_t>(arpeggiator.getShape());
    profile.arp.swingPercent =
        static_cast<uint8_t>(constrain(arpeggiator.getSwingPercent(), 0.0f, 80.0f));
    profile.arp.gatePercent =
        static_cast<uint8_t>(constrain(arpeggiator.getGatePercent(), 5.0f, 100.0f));
    profile.arp.octaveRange = arpeggiator.getOctaveRange();
    profile.arp.patternLength = arpeggiator.getPatternLength();
    for (size_t i = 0; i < Arpeggiator::ASSIGNMENT_BYTES; ++i) {
        profile.arp.assignedSlots[i] = arpeggiator.getAssignmentByte(i);
    }

    profile.led.brightness = ledManager.getBrightness();
    CRGB color = ledManager.getColor();
    profile.led.r = color.r;
    profile.led.g = color.g;
    profile.led.b = color.b;
    profile.clock.tappedBpm = g_tappedBPM;
    profile.clock.clockOutEnabled = g_clockOutEnabled ? 1 : 0;
    profile.clock.followExternalClock = g_followExternalClock ? 1 : 0;
    profile.noteDynamics.velocityShift = velocityShift;
    profile.noteDynamics.changeProbability = changeProbability;
    profile.jitter.depth = g_jitterSettings.depth;
    profile.jitter.smoothness = g_jitterSettings.smoothness;

    for (uint8_t i = 0; i < PROFILE_LFO_COUNT; ++i) {
        LFO &lfo = lfoManager.lfo(i);
        profile.lfos[i].shape = static_cast<uint8_t>(lfo.getShape());
        profile.lfos[i].frequencyHz = lfo.getFrequencyHz();
        profile.lfos[i].depth = lfo.getDepth();
        profile.lfos[i].bipolar = lfo.isBipolar() ? 1 : 0;
        profile.lfos[i].syncEnabled = lfo.isSyncEnabled() ? 1 : 0;
        profile.lfos[i].syncRatio = static_cast<uint8_t>(lfo.getSyncRatio());
    }

    const size_t routeCount =
        std::min(lfoManager.routeCount(), static_cast<size_t>(PROFILE_MAX_ROUTES));
    profile.routeCount = static_cast<uint8_t>(routeCount);
    for (size_t i = 0; i < routeCount; ++i) {
        LFOManager::Route route{};
        if (!lfoManager.getRoute(i, route)) {
            continue;
        }
        profile.routes[i].type = static_cast<uint8_t>(route.type);
        profile.routes[i].lfoIndex = route.lfoIndex;
        profile.routes[i].depth = route.depth;
        profile.routes[i].target = route.type == LFOManager::Route::Type::SlotValue
                                       ? route.slotIndex
                                       : static_cast<uint8_t>(route.target);
        profile.routes[i].channel = route.channel;
        profile.routes[i].ccMsb = route.ccMsb;
        profile.routes[i].ccLsb = route.ccLsb;
        profile.routes[i].amount = route.amount;
        profile.routes[i].minValue = route.minValue;
        profile.routes[i].maxValue = route.maxValue;
    }

    const auto &slots = configManager.getSlots();
    for (uint8_t i = 0; i < NUM_SLOTS; ++i) {
        profile.slots[i].midiChannel = slots[i].midiChannel;
        profile.slots[i].followerIndex = slots[i].getEnvelopeFollowerIndex();
        profile.slots[i].ef = profileEfFromSlot(slots[i].efSettings);
    }

    return profile;
}

ProfileModulationExtension captureProfileModulation() {
    ProfileModulationExtension extension{};
    const auto &slots = configManager.getSlots();
    for (uint8_t i = 0; i < NUM_SLOTS; ++i) {
        extension.slots[i].argPacked = packProfileSlotArg(slots[i].arg);
        for (uint8_t lane = 0; lane < PROFILE_LFO_COUNT; ++lane) {
            extension.slots[i].lfo[lane] = slots[i].lfo.lfo[lane];
        }
    }
    return sanitizeProfileModulation(extension);
}

ProfileModulationExtension defaultProfileModulationSnapshot() {
    ProfileModulationExtension extension{};
    const SlotARGConfig defaultArg{};
    for (ProfileSlotModSettings &slot : extension.slots) {
        slot.argPacked = packProfileSlotArg(defaultArg);
    }
    return sanitizeProfileModulation(extension);
}

bool persistActiveProfileSnapshot() {
    return configManager.saveProfileSettings(g_activeProfile, captureProfileSnapshot()) &&
           configManager.saveProfileModulation(g_activeProfile, captureProfileModulation());
}

// 3b. Restore a previously captured profile into the live engine state. Callers can choose
// whether that restore should also be committed back to EEPROM.
void applyProfileSnapshot(const ProfileData &profile, bool persistSlots) {
    // `persistSlots` lets callers apply an in-memory profile preview without rewriting EEPROM
    // until the user commits.
    arpeggiator.setLength(profile.arp.lengthTicks);
    arpeggiator.setShape(static_cast<Arpeggiator::Shape>(profile.arp.shape));
    arpeggiator.setSwingPercent(profile.arp.swingPercent);
    arpeggiator.setGatePercent(profile.arp.gatePercent);
    arpeggiator.setOctaveRange(profile.arp.octaveRange);
    arpeggiator.setPatternLength(profile.arp.patternLength);
    arpeggiator.clearAssignments();
    for (size_t i = 0; i < Arpeggiator::ASSIGNMENT_BYTES; ++i) {
        arpeggiator.setAssignmentByte(i, profile.arp.assignedSlots[i]);
    }

    ledManager.setBrightness(profile.led.brightness);
    ledManager.setColor(CRGB(profile.led.r, profile.led.g, profile.led.b));
    g_tappedBPM = profile.clock.tappedBpm;
    g_clockOutEnabled = profile.clock.clockOutEnabled != 0;
    g_followExternalClock = profile.clock.followExternalClock != 0;
    velocityShift = profile.noteDynamics.velocityShift;
    changeProbability = profile.noteDynamics.changeProbability;
    g_noteDynamicsRemoteControlActive = false;
    g_noteDynamicsShiftLatched = false;
    g_noteDynamicsProbabilityLatched = false;
    g_jitterSettings.depth = profile.jitter.depth;
    g_jitterSettings.smoothness = profile.jitter.smoothness;
    g_jitterRemoteControlActive = false;
    g_jitterDepthLatched = false;
    g_jitterSmoothnessLatched = false;

    lfoManager.applyProfile(profile);

    potToEnvelopeMap.clear();
    for (uint8_t i = 0; i < NUM_SLOTS; ++i) {
        MIDISlot &slot = configManager.getSlot(i);
        slot.midiChannel = profile.slots[i].midiChannel;
        applyProfileEfToSlot(profile.slots[i].ef, slot.efSettings);
        slot.setEnvelopeFollowerIndex(profile.slots[i].followerIndex);
        if (slot.getEnvelopeFollowerIndex() >= 0) {
            potToEnvelopeMap[i] = slot.efSettings;
        }
        if (persistSlots) {
            configManager.saveSlot(i, slot);
        }
    }

    for (const auto &entry : potToEnvelopeMap) {
        const int followerIndex = entry.second.followerIndex;
        if (followerIndex >= 0 && followerIndex < static_cast<int>(envelopeFollowers.size())) {
            applyEfSettingsToFollower(envelopeFollowers[followerIndex], entry.second,
                                      static_cast<uint8_t>(followerIndex));
        }
    }

    if (persistSlots) {
        configManager.saveLEDSettings(ledManager.getBrightness(), ledManager.getColor());
        configManager.saveEnvelopeSettings(potToEnvelopeMap, envelopeFollowers);
    }
}

void applyProfileModulation(const ProfileModulationExtension &candidate, bool persistSlots) {
    const ProfileModulationExtension extension = sanitizeProfileModulation(candidate);
    for (uint8_t i = 0; i < NUM_SLOTS; ++i) {
        MIDISlot &slot = configManager.getSlot(i);
        slot.arg = unpackProfileSlotArg(extension.slots[i].argPacked);
        for (uint8_t lane = 0; lane < PROFILE_LFO_COUNT; ++lane) {
            slot.lfo.lfo[lane] = extension.slots[i].lfo[lane];
        }
        slot.lfo = sanitizeSlotLfoConfig(slot.lfo);
        if (persistSlots) configManager.saveSlot(i, slot);
    }
}

void applyCompleteProfile(const ProfileData &profile,
                          const ProfileModulationExtension &extension,
                          bool persistSlots) {
    // Compose both halves in memory before persistence so a profile transition writes each slot
    // once and storage never observes an intermediate MIDI/EF-only slot snapshot.
    applyProfileSnapshot(profile, false);
    applyProfileModulation(extension, false);
    if (!persistSlots) return;

    for (uint8_t i = 0; i < NUM_SLOTS; ++i) {
        configManager.saveSlot(i, configManager.getSlot(i));
    }
    configManager.saveLEDSettings(ledManager.getBrightness(), ledManager.getColor());
    configManager.saveEnvelopeSettings(potToEnvelopeMap, envelopeFollowers);
}

// 4a. Seed the two onboard LFOs with a predictable default routing so a clean boot still has a
// coherent modulation story before any profile or host edits arrive.
void configureLFOs() {
    lfoManager.clearRoutes();

    LFO &lfo1 = lfoManager.lfo(0);
    lfo1.setShape(LFOShape::Sine);
    lfo1.setFrequencyHz(1.0f);
    lfo1.setDepth(0.0f);
    lfo1.setBipolar(false);
    lfo1.setSyncEnabled(false);
    lfo1.setSyncRatio(LFOSyncRatio::Div1);

    LFO &lfo2 = lfoManager.lfo(1);
    lfo2.setShape(LFOShape::Triangle);
    lfo2.setFrequencyHz(0.5f);
    lfo2.setDepth(0.0f);
    lfo2.setBipolar(true);
    lfo2.setSyncEnabled(false);
    lfo2.setSyncRatio(LFOSyncRatio::Div1);

    lfoManager.addInternalRoute(0, LFOInternalTarget::ArpSwing, 1.0f);
    lfoManager.addInternalRoute(1, LFOInternalTarget::EfGainTrim, 1.0f);
    lfoManager.addInternalRoute(1, LFOInternalTarget::VelocityShift, 0.5f);
}

void restoreActiveProfileRuntime(bool persistSnapshot) {
    configureLFOs();

    g_activeProfile = configManager.getActiveProfile();
    if (g_activeProfile >= NUM_PROFILES) {
        g_activeProfile = 0;
        configManager.setActiveProfile(g_activeProfile);
    }
    configManager.loadProfile(g_activeProfile);
    potChannels.clear();
    for (uint8_t i = 0; i < configManager.getNumPots(); ++i) {
        potChannels.push_back(configManager.getPotChannel(i));
    }

    ProfileData storedProfile{};
    const bool profileStored = configManager.loadProfileSettings(g_activeProfile, storedProfile);
    ProfileModulationExtension modulation{};
    if (!configManager.loadProfileModulation(g_activeProfile, modulation)) {
        // Schema-7 profiles inherited the formerly global slot modulation on
        // first load, preserving their audible behavior after the upgrade.
        modulation = captureProfileModulation();
        configManager.saveProfileModulation(g_activeProfile, modulation);
    }
    if (profileStored) {
        applyCompleteProfile(storedProfile, modulation, persistSnapshot);
    } else {
        applyProfileModulation(modulation, persistSnapshot);
    }
    refreshEfVoicesFromConfig();
}

// 4b. Rebuild the envelope-follower voice cache after profile loads or recovery paths mutate the
// underlying slot-to-follower mappings.
void refreshEfVoicesFromConfig() {
    for (uint8_t slotIndex = 0; slotIndex < NUM_SLOTS; ++slotIndex) {
        EfVoice &voice = efVoices[slotIndex];
        const MIDISlot &slot = configManager.getSlot(slotIndex);
        auto mapIt = potToEnvelopeMap.find(slotIndex);
        if (mapIt != potToEnvelopeMap.end()) {
            voice.assignFollower(mapIt->second.followerIndex);
        } else {
            voice.resetFollower();
        }
        voice.syncSettings(slot.efSettings);
    }
}

// 4c. Reconstruct all profile-driven runtime state during boot: follower baselines, default LFO
// state, active profile selection, and the channel cache still read directly by some UI/transport
// paths.
bool initializeModes() {
    // Load active profile wiring first, then rebuild the channel cache consumed by UI/transport
    // paths that still read `potChannels` directly.
    bool baselinesLoaded = configManager.loadEnvelopeSettings(potToEnvelopeMap, envelopeFollowers);
    refreshEfVoicesFromConfig();
    for (size_t i = 0; i < envelopeFollowers.size(); ++i) {
        if (i < efBaseGains.size()) {
            efBaseGains[i] = envelopeFollowers[i].getGain();
        }
    }
    restoreActiveProfileRuntime(true);
    return baselinesLoaded;
}

// 5. Keep the shared `const char*` label pointer valid by storing edits in the backing `String`
// and then repointing consumers at its internal buffer.
void updateEnvelopeModeLabel(const char *label) {
    if (!label || label[0] == '\0') {
        g_envelopeModeLabel = "LINEAR";
    } else {
        g_envelopeModeLabel = label;
    }
    envelopeMode = g_envelopeModeLabel.c_str();
}
