#include "ProfileModulationStorage.h"

#include <cstddef>

#ifndef FLASHMEM
#define FLASHMEM
#endif

namespace {
uint16_t crc16Update(uint16_t crc, uint8_t data) {
    crc ^= data;
    for (uint8_t i = 0; i < 8; ++i) {
        crc = (crc & 1) ? static_cast<uint16_t>((crc >> 1) ^ 0xA001)
                        : static_cast<uint16_t>(crc >> 1);
    }
    return crc;
}
} // namespace

FLASHMEM uint16_t packProfileSlotArg(const SlotARGConfig &candidate) {
    const SlotARGConfig arg = sanitizeSlotArg(candidate);
    return static_cast<uint16_t>((arg.enabled ? 1U : 0U) |
                                 ((static_cast<uint16_t>(arg.method) & 0x0FU) << 1U) |
                                 ((static_cast<uint16_t>(arg.sourceA) & 0x07U) << 5U) |
                                 ((static_cast<uint16_t>(arg.sourceB) & 0x07U) << 8U));
}

FLASHMEM SlotARGConfig unpackProfileSlotArg(uint16_t packed) {
    SlotARGConfig arg{};
    arg.enabled = static_cast<uint8_t>(packed & 0x01U);
    arg.method = static_cast<ARGMethod>((packed >> 1U) & 0x0FU);
    arg.sourceA = static_cast<uint8_t>((packed >> 5U) & 0x07U);
    arg.sourceB = static_cast<uint8_t>((packed >> 8U) & 0x07U);
    return sanitizeSlotArg(arg);
}

FLASHMEM ProfileModulationExtension sanitizeProfileModulation(
    const ProfileModulationExtension &candidate) {
    ProfileModulationExtension extension = candidate;
    extension.version = PROFILE_MODULATION_VERSION;
    for (ProfileSlotModSettings &slot : extension.slots) {
        slot.argPacked = packProfileSlotArg(unpackProfileSlotArg(slot.argPacked));
        for (SlotLfoLane &lane : slot.lfo) lane = sanitizeSlotLfoLane(lane);
    }
    return extension;
}

FLASHMEM uint16_t computeProfileModulationCrc(const ProfileModulationExtension &extension) {
    constexpr size_t start = offsetof(ProfileModulationExtension, slots);
    const uint8_t *bytes = reinterpret_cast<const uint8_t *>(&extension);
    uint16_t crc = 0xFFFF;
    for (size_t i = start; i < sizeof(ProfileModulationExtension); ++i) {
        crc = crc16Update(crc, bytes[i]);
    }
    return crc;
}
