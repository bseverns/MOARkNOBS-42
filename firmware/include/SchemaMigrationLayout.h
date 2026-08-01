#ifndef SCHEMA_MIGRATION_LAYOUT_H
#define SCHEMA_MIGRATION_LAYOUT_H

#include "StorageLayout.h"

#include <cstddef>

struct Schema6StorageLayout {
    size_t oldConfigTail = 0;
    size_t newConfigTail = 0;
    size_t tailShift = 0;
    size_t oldProfileSettings = 0;
    size_t oldMacroStorage = 0;
    size_t oldSceneStorage = 0;
    size_t oldRequiredStorage = 0;
};

constexpr Schema6StorageLayout schema6StorageLayout(size_t legacySlotBytes,
                                                    size_t legacyMacroBytes,
                                                    size_t legacySceneBytes,
                                                    size_t sceneCount) {
    const size_t oldSlotRegionBytes = legacySlotBytes * NUM_SLOTS;
    const size_t oldConfigTail = EEPROM_SLOT_BASE + oldSlotRegionBytes +
                                 sizeof(float) * 2U + sizeof(uint16_t);
    const size_t oldProfileSettings =
        oldConfigTail + NUM_PROFILES * EEPROM_PROFILE_BLOCK_SIZE;
    const size_t oldMacroStorage =
        oldProfileSettings + NUM_PROFILES * EEPROM_PROFILE_SETTINGS_BLOCK_SIZE;
    const size_t oldSceneStorage = oldMacroStorage + legacyMacroBytes;
    return {
        oldConfigTail,
        EEPROM_CONFIG_TAIL,
        EEPROM_CONFIG_TAIL - oldConfigTail,
        oldProfileSettings,
        oldMacroStorage,
        oldSceneStorage,
        oldSceneStorage + sceneCount * legacySceneBytes,
    };
}

struct Schema7StorageLayout {
    size_t oldMacroStorage = EEPROM_PROFILE_MODULATION_BASE;
    size_t newMacroStorage = EEPROM_PROFILE_MODULATION_START(NUM_PROFILES);
    size_t tailShift = newMacroStorage - oldMacroStorage;
};

constexpr Schema7StorageLayout schema7StorageLayout() { return {}; }

#endif // SCHEMA_MIGRATION_LAYOUT_H
