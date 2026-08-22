#include "protocol/ConfigApplyDigest.h"

#include <Arduino.h>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "ConfigManager.h"
#include "EnvelopeFollower.h"
#include "FirmwareState.h"
#include "Globals.h"
#include "Modes.h"

// Canonical device-owned digest for the normalized state committed by SET_ALL.
// Host checksums remain transport correlation tokens and are never trusted as
// proof of the applied runtime/persistence state.
namespace ConfigApplyDigest {
namespace {
void hashByte(uint32_t &hash, uint8_t value) {
    hash ^= value;
    hash *= 16777619UL;
}

void hashU16(uint32_t &hash, uint16_t value) {
    hashByte(hash, static_cast<uint8_t>(value));
    hashByte(hash, static_cast<uint8_t>(value >> 8));
}

void hashFloat(uint32_t &hash, float value) {
    // Canonicalize signed zero, then hash IEEE-754 bytes in a fixed order.
    if (value == 0.0f) value = 0.0f;
    uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    for (uint8_t shift = 0; shift < 32; shift += 8) {
        hashByte(hash, static_cast<uint8_t>(bits >> shift));
    }
}

void hashSlot(uint32_t &hash, const MIDISlot &slot) {
    hashByte(hash, static_cast<uint8_t>(slot.type));
    hashByte(hash, slot.midiChannel); hashByte(hash, slot.data1); hashByte(hash, slot.active ? 1 : 0);
    hashByte(hash, slot.arpNote); hashByte(hash, slot.sysexLength);
    for (uint8_t value : slot.sysexTemplate) hashByte(hash, value);
    const auto &ef = slot.efSettings;
    hashByte(hash, static_cast<uint8_t>(ef.followerIndex)); hashByte(hash, ef.oversample);
    hashByte(hash, static_cast<uint8_t>(ef.filterType)); hashByte(hash, ef.efMode);
    hashByte(hash, ef.autoBaseline); hashByte(hash, ef.autoGain); hashByte(hash, ef.gateThreshold);
    hashByte(hash, ef.gateHysteresis); hashByte(hash, ef.activityThreshold); hashByte(hash, ef.gainTarget);
    hashByte(hash, ef.destinationMode); hashU16(hash, ef.attackMs); hashU16(hash, ef.releaseMs);
    hashU16(hash, ef.rmsWindowMs); hashU16(hash, ef.baselineTauMs); hashU16(hash, ef.gainTauMs);
    hashFloat(hash, ef.frequency); hashFloat(hash, ef.q); hashFloat(hash, ef.smoothing);
    hashFloat(hash, ef.baseline); hashFloat(hash, ef.gain);
}

void hashProfileModulation(uint32_t &hash,
                           const ProfileModulationExtension &modulation) {
    // Hash the sanitized semantic payload in its persistence order. The CRC
    // is derived data and raw struct bytes could include layout padding.
    hashU16(hash, modulation.version);
    for (const ProfileSlotModSettings &slot : modulation.slots) {
        hashU16(hash, slot.argPacked);
        for (const SlotLfoLane &lane : slot.lfo) {
            hashByte(hash, lane.flags);
            hashByte(hash, static_cast<uint8_t>(lane.amount));
        }
    }
    hashByte(hash, modulation.midiInputBindingCount);
    for (uint8_t i = 0; i < modulation.midiInputBindingCount; ++i) {
        const MidiInputBinding &binding = modulation.midiInputBindings[i];
        hashByte(hash, binding.port);
        hashByte(hash, binding.channel);
        hashByte(hash, binding.controller);
        hashByte(hash, binding.target);
        hashByte(hash, binding.targetIndex);
        hashByte(hash, binding.mode);
        hashByte(hash, binding.minValue);
        hashByte(hash, binding.maxValue);
        hashByte(hash, binding.flags);
    }
}

// Device-owned digest over the normalized slot arena and the semantic profile
// snapshots written by persistActiveProfileSnapshot(). The host checksum
// remains only a correlation token.
} // namespace

String computeAppliedStateChecksum() {
    uint32_t hash = 2166136261UL; // FNV-1a, stable and small enough for Teensy.
    for (uint8_t i = 0; i < configManager.getNumPots(); ++i) {
        hashByte(hash, configManager.getPotChannel(i));
        hashByte(hash, configManager.getPotCCNumber(i));
    }
    for (const MIDISlot &slot : configManager.getSlots()) hashSlot(hash, slot);
    const ProfileData profile = captureProfileSnapshot();
    const auto *profileBytes = reinterpret_cast<const uint8_t *>(&profile);
    for (size_t i = 0; i < sizeof(profile); ++i) hashByte(hash, profileBytes[i]);
    hashProfileModulation(hash, captureProfileModulation());
    hashByte(hash, g_activeProfile);
    hashByte(hash, configManager.getARGEnable());
    hashByte(hash, configManager.getARGMethod());
    hashByte(hash, configManager.getEnvelopeA());
    hashByte(hash, configManager.getEnvelopeB());
    hashByte(hash, configManager.getMode());
    hashByte(hash, configManager.getEfIdleFloor());
    hashByte(hash, static_cast<uint8_t>(configManager.getLedMode()));
    float filterFrequency = 0.0f;
    float filterQ = 0.0f;
    StorageBackend *storage = ConfigManager::getStorageBackend();
    storage->readBytes(EEPROM_FILTER_FREQ, &filterFrequency, sizeof(filterFrequency));
    storage->readBytes(EEPROM_FILTER_Q, &filterQ, sizeof(filterQ));
    hashFloat(hash, filterFrequency);
    hashFloat(hash, filterQ);
    hashByte(hash, envelopeFollowers.empty()
                       ? static_cast<uint8_t>(EnvelopeFollower::LINEAR)
                       : static_cast<uint8_t>(envelopeFollowers.front().getFilterType()));
    for (float baseline : envelopeConfig.baselines) hashFloat(hash, baseline);
    hashByte(hash, g_usbMidiOutEnabled ? 1 : 0);
    char hex[9] = {0};
    snprintf(hex, sizeof(hex), "%08lx", static_cast<unsigned long>(hash));
    return String(hex);
}
} // namespace ConfigApplyDigest
