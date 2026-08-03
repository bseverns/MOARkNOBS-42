// ProfileStorage.cpp — Profile payload sanitization and CRC.
//
// Extracted from ConfigManager.cpp so profile persistence logic is testable
// and readable in isolation. ConfigManager::loadProfileSettings and
// saveProfileSettings call these helpers.

#include "ProfileStorage.h"
#include "BoardPowerProfile.h"
#include "EnvelopeFollower.h"
#include "Arpeggiator.h"
#include "LFO/LFOManager.h"
#include <cmath>
#include <cstddef>

namespace {

// CRC-16 with the Modbus-flavored 0xA001 polynomial. Shared with
// ConfigManager's core CRC but duplicated here to keep the translation
// unit self-contained.
uint16_t crc16_update_profile(uint16_t crc, uint8_t data) {
    crc ^= data;
    for (uint8_t i = 0; i < 8; ++i) {
        if (crc & 1) {
            crc = (crc >> 1) ^ 0xA001;
        } else {
            crc >>= 1;
        }
    }
    return crc;
}

} // namespace

ProfileEfSettings sanitizeProfileEfSettings(const ProfileEfSettings &settings) {
    ProfileEfSettings sanitized = settings;
    if (sanitized.mode > static_cast<uint8_t>(EnvelopeFollower::EFMode::Follower)) {
        sanitized.mode = static_cast<uint8_t>(EnvelopeFollower::EFMode::Peak);
    }
    sanitized.autoBaseline = sanitized.autoBaseline ? 1 : 0;
    sanitized.autoGain = sanitized.autoGain ? 1 : 0;
    sanitized.gateThreshold = constrain(sanitized.gateThreshold, 0, 127);
    sanitized.gateHysteresis = constrain(sanitized.gateHysteresis, 0, 127);
    sanitized.activityThreshold = constrain(sanitized.activityThreshold, 0, 127);
    sanitized.gainTarget = constrain(sanitized.gainTarget, 0, 127);
    if (sanitized.destinationMode > static_cast<uint8_t>(EfDestinationMode::Centered)) {
        sanitized.destinationMode = static_cast<uint8_t>(EfDestinationMode::AddClamp);
    }
    sanitized.attackMs = static_cast<uint16_t>(constrain(static_cast<int>(sanitized.attackMs),
                                                         static_cast<int>(EF_TIME_MIN_MS),
                                                         static_cast<int>(EF_TIME_MAX_MS)));
    sanitized.releaseMs = static_cast<uint16_t>(constrain(static_cast<int>(sanitized.releaseMs),
                                                          static_cast<int>(EF_TIME_MIN_MS),
                                                          static_cast<int>(EF_TIME_MAX_MS)));
    sanitized.rmsWindowMs = static_cast<uint16_t>(constrain(static_cast<int>(sanitized.rmsWindowMs),
                                                            static_cast<int>(EF_TIME_MIN_MS),
                                                            static_cast<int>(EF_TIME_MAX_MS)));
    sanitized.baselineTauMs = static_cast<uint16_t>(
        constrain(static_cast<int>(sanitized.baselineTauMs), static_cast<int>(EF_TIME_MIN_MS),
                  static_cast<int>(EF_TIME_MAX_MS)));
    sanitized.gainTauMs = static_cast<uint16_t>(constrain(static_cast<int>(sanitized.gainTauMs),
                                                          static_cast<int>(EF_TIME_MIN_MS),
                                                          static_cast<int>(EF_TIME_MAX_MS)));
    return sanitized;
}

ProfileData sanitizeProfileData(const ProfileData &profile) {
    ProfileData sanitized = profile;
    sanitized.version = PROFILE_SETTINGS_VERSION;
    // Arp
    if (sanitized.arp.shape > static_cast<uint8_t>(Arpeggiator::EUCLIDEAN)) {
        sanitized.arp.shape = static_cast<uint8_t>(Arpeggiator::UP);
    }
    sanitized.arp.swingPercent = constrain(sanitized.arp.swingPercent, 0, 80);
    sanitized.arp.gatePercent = constrain(sanitized.arp.gatePercent, 5, 100);
    sanitized.arp.octaveRange = constrain(sanitized.arp.octaveRange, 0, 3);
    sanitized.arp.patternLength =
        constrain(sanitized.arp.patternLength, Arpeggiator::MIN_PATTERN_LENGTH,
                  Arpeggiator::MAX_PATTERN_LENGTH);
    if (sanitized.arp.lengthTicks == 0) {
        sanitized.arp.lengthTicks = 12;
    }
    if (NUM_SLOTS % 8U != 0U) {
        sanitized.arp.assignedSlots[Arpeggiator::ASSIGNMENT_BYTES - 1U] &=
            static_cast<uint8_t>((1U << (NUM_SLOTS % 8U)) - 1U);
    }
    // LED
    sanitized.led.brightness =
        constrain(sanitized.led.brightness, 0, BoardPowerProfile::kLedBrightnessCap);
    // Clock
    if (!std::isfinite(sanitized.clock.tappedBpm)) {
        sanitized.clock.tappedBpm = 120.0f;
    }
    sanitized.clock.tappedBpm = constrain(sanitized.clock.tappedBpm, 20.0f, 300.0f);
    sanitized.clock.clockOutEnabled = sanitized.clock.clockOutEnabled ? 1 : 0;
    sanitized.clock.followExternalClock = sanitized.clock.followExternalClock ? 1 : 0;
    // Note dynamics
    sanitized.noteDynamics.velocityShift = static_cast<int8_t>(
        constrain(static_cast<int>(sanitized.noteDynamics.velocityShift), -64, 63));
    sanitized.noteDynamics.changeProbability =
        static_cast<uint8_t>(constrain(sanitized.noteDynamics.changeProbability, 0, 100));
    // Jitter
    if (!std::isfinite(sanitized.jitter.depth)) {
        sanitized.jitter.depth = 1.0f;
    }
    if (!std::isfinite(sanitized.jitter.smoothness)) {
        sanitized.jitter.smoothness = 0.5f;
    }
    sanitized.jitter.depth = constrain(sanitized.jitter.depth, 0.0f, 1.0f);
    sanitized.jitter.smoothness = constrain(sanitized.jitter.smoothness, 0.0f, 1.0f);
    // LFO
    if (sanitized.routeCount > PROFILE_MAX_ROUTES) {
        sanitized.routeCount = PROFILE_MAX_ROUTES;
    }
    for (auto &lfo : sanitized.lfos) {
        if (lfo.shape > static_cast<uint8_t>(LFOShape::RandomSlew)) {
            lfo.shape = static_cast<uint8_t>(LFOShape::Sine);
        }
        if (!std::isfinite(lfo.frequencyHz) || lfo.frequencyHz < 0.0f) {
            lfo.frequencyHz = 0.0f;
        }
        if (!std::isfinite(lfo.depth)) {
            lfo.depth = 0.0f;
        }
        lfo.depth = constrain(lfo.depth, 0.0f, 1.0f);
        lfo.bipolar = lfo.bipolar ? 1 : 0;
        lfo.syncEnabled = lfo.syncEnabled ? 1 : 0;
        if (lfo.syncRatio > static_cast<uint8_t>(LFOSyncRatio::Mul4)) {
            lfo.syncRatio = static_cast<uint8_t>(LFOSyncRatio::Div1);
        }
    }
    for (uint8_t i = sanitized.routeCount; i < PROFILE_MAX_ROUTES; ++i) {
        sanitized.routes[i] = ProfileLfoRoute{};
    }
    for (uint8_t i = 0; i < sanitized.routeCount; ++i) {
        ProfileLfoRoute &route = sanitized.routes[i];
        if (route.type > static_cast<uint8_t>(LFOManager::Route::Type::SlotValue)) {
            route.type = static_cast<uint8_t>(LFOManager::Route::Type::Internal);
        }
        if (route.lfoIndex >= PROFILE_LFO_COUNT) {
            route.lfoIndex = 0;
        }
        if (!std::isfinite(route.depth)) {
            route.depth = 0.0f;
        }
        route.depth = constrain(route.depth, 0.0f, 1.0f);
        route.amount = static_cast<int8_t>(constrain(static_cast<int>(route.amount), -100, 100));
        route.minValue = static_cast<uint8_t>(constrain(route.minValue, 0, 127));
        route.maxValue = static_cast<uint8_t>(constrain(route.maxValue, 0, 127));
        if (route.minValue > route.maxValue) {
            uint8_t tmp = route.minValue;
            route.minValue = route.maxValue;
            route.maxValue = tmp;
        }
        if (route.type == static_cast<uint8_t>(LFOManager::Route::Type::SlotValue)) {
            route.target = static_cast<uint8_t>(constrain(route.target, 0, NUM_SLOTS - 1));
        } else if (route.target > static_cast<uint8_t>(LFOInternalTarget::JitterSmoothness)) {
            route.target = static_cast<uint8_t>(LFOInternalTarget::EfGainTrim);
        }
        route.channel = static_cast<uint8_t>(constrain(route.channel, 1, 16));
    }
    for (auto &slot : sanitized.slots) {
        if (slot.midiChannel < 1 || slot.midiChannel > 16) {
            slot.midiChannel = 1;
        }
        if (slot.followerIndex < -1 || slot.followerIndex >= static_cast<int8_t>(NUM_ENVELOPES)) {
            slot.followerIndex = -1;
        }
        slot.ef = sanitizeProfileEfSettings(slot.ef);
    }
    return sanitized;
}

uint16_t computeProfileCrc(const ProfileData &profile) {
    constexpr size_t kCrcStart = offsetof(ProfileData, routeCount);
    const uint8_t *bytes = reinterpret_cast<const uint8_t *>(&profile);
    uint16_t crc = 0xFFFF;
    for (size_t i = kCrcStart; i < sizeof(ProfileData); ++i) {
        crc = crc16_update_profile(crc, bytes[i]);
    }
    return crc;
}
