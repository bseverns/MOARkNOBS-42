#ifndef STORAGE_LAYOUT_H
#define STORAGE_LAYOUT_H

#include "MIDITypes.h"

#include <cstddef>
#include <cstdint>

// Arduino-independent EEPROM address contract. Keeping this arithmetic outside Globals.h lets
// native tests prove that profile/modulation blocks remain adjacent and non-overlapping.
inline constexpr std::size_t SLOT_EEPROM_SIZE = sizeof(MIDISlot);
inline constexpr uint8_t NUM_PROFILES = 4;
inline constexpr uint16_t EEPROM_PROFILE_BLOCK_SIZE = 256;
inline constexpr uint16_t EEPROM_PROFILE_SETTINGS_BLOCK_SIZE = 1024;
inline constexpr uint16_t EEPROM_START_ADDRESS = 0;
inline constexpr uint16_t EEPROM_MAGIC_ADDRESS = EEPROM_START_ADDRESS + 200;
inline constexpr uint16_t EEPROM_MAGIC_PRIMARY = 0xABCD;
inline constexpr uint16_t EEPROM_MAGIC_BACKUP = 0xDCBA;

inline constexpr uint8_t STORAGE_LAYOUT_NUM_ENVELOPES = 6;
inline constexpr uint16_t EEPROM_EF_BASELINES = EEPROM_MAGIC_ADDRESS + 4;
inline constexpr uint16_t EEPROM_EF_BASELINES_SIZE =
    STORAGE_LAYOUT_NUM_ENVELOPES * sizeof(float);
inline constexpr uint8_t EEPROM_BUFFER_SIZE = 22;
inline constexpr uint16_t EEPROM_BACKUP_START =
    EEPROM_EF_BASELINES + EEPROM_EF_BASELINES_SIZE + EEPROM_BUFFER_SIZE;
inline constexpr uint16_t EEPROM_CONFIG_MIRROR_SIZE = EEPROM_BACKUP_START * 2;
inline constexpr uint16_t EEPROM_SLOT_BASE = EEPROM_CONFIG_MIRROR_SIZE;
inline constexpr uint16_t EEPROM_SLOT_REGION_SIZE =
    static_cast<uint16_t>(SLOT_EEPROM_SIZE * NUM_SLOTS);

inline constexpr uint16_t EEPROM_LEGACY_FILTER_FREQ = 1000;
inline constexpr uint16_t EEPROM_LEGACY_FILTER_Q = 1004;
inline constexpr uint16_t EEPROM_LEGACY_BROWNOUT_COUNT = 1008;
inline constexpr uint16_t EEPROM_FILTER_FREQ =
    static_cast<uint16_t>(EEPROM_SLOT_BASE + EEPROM_SLOT_REGION_SIZE);
inline constexpr uint16_t EEPROM_FILTER_Q =
    static_cast<uint16_t>(EEPROM_FILTER_FREQ + sizeof(float));
inline constexpr uint16_t EEPROM_BROWNOUT_COUNT =
    static_cast<uint16_t>(EEPROM_FILTER_Q + sizeof(float));
inline constexpr uint16_t EEPROM_CONFIG_TAIL =
    static_cast<uint16_t>(EEPROM_BROWNOUT_COUNT + sizeof(uint16_t));

inline constexpr uint16_t EEPROM_PROFILE_START(uint8_t id) {
    return id == 0
               ? EEPROM_START_ADDRESS
               : static_cast<uint16_t>(EEPROM_CONFIG_TAIL +
                                       (id - 1U) * EEPROM_PROFILE_BLOCK_SIZE);
}

inline constexpr uint16_t EEPROM_PROFILE_SETTINGS_BASE =
    static_cast<uint16_t>(EEPROM_CONFIG_TAIL + NUM_PROFILES * EEPROM_PROFILE_BLOCK_SIZE);

inline constexpr uint16_t EEPROM_PROFILE_SETTINGS_START(uint8_t id) {
    return static_cast<uint16_t>(EEPROM_PROFILE_SETTINGS_BASE +
                                 id * EEPROM_PROFILE_SETTINGS_BLOCK_SIZE);
}

inline constexpr uint16_t EEPROM_PROFILE_MODULATION_BLOCK_SIZE = 512;
inline constexpr uint16_t EEPROM_PROFILE_MODULATION_BASE =
    EEPROM_PROFILE_SETTINGS_START(NUM_PROFILES);

inline constexpr uint16_t EEPROM_PROFILE_MODULATION_START(uint8_t id) {
    return static_cast<uint16_t>(EEPROM_PROFILE_MODULATION_BASE +
                                 id * EEPROM_PROFILE_MODULATION_BLOCK_SIZE);
}

inline constexpr uint16_t EEPROM_SYSTEM_FLAGS_BASE =
    static_cast<uint16_t>(EEPROM_BACKUP_START - sizeof(uint32_t));
inline constexpr uint16_t EEPROM_USB_CONFIG_BOOT_REQUEST = EEPROM_SYSTEM_FLAGS_BASE;

#endif // STORAGE_LAYOUT_H
