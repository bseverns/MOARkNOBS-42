#include <unity.h>

#include "SchemaMigrationLayout.h"
#include "ProfileModulationStorage.h"

void test_native_storage_regions_are_contiguous_and_non_overlapping() {
    TEST_ASSERT_EQUAL_UINT32(EEPROM_SLOT_BASE + SLOT_EEPROM_SIZE * NUM_SLOTS,
                             EEPROM_FILTER_FREQ);
    TEST_ASSERT_EQUAL_UINT32(EEPROM_CONFIG_TAIL + NUM_PROFILES * EEPROM_PROFILE_BLOCK_SIZE,
                             EEPROM_PROFILE_SETTINGS_BASE);
    TEST_ASSERT_EQUAL_UINT32(EEPROM_PROFILE_SETTINGS_START(NUM_PROFILES),
                             EEPROM_PROFILE_MODULATION_BASE);
    TEST_ASSERT_EQUAL_UINT32(
        EEPROM_PROFILE_MODULATION_BASE +
            NUM_PROFILES * EEPROM_PROFILE_MODULATION_BLOCK_SIZE,
        EEPROM_PROFILE_MODULATION_START(NUM_PROFILES));
    for (uint8_t id = 0; id + 1U < NUM_PROFILES; ++id) {
        TEST_ASSERT_EQUAL_UINT32(EEPROM_PROFILE_SETTINGS_START(id) +
                                     EEPROM_PROFILE_SETTINGS_BLOCK_SIZE,
                                 EEPROM_PROFILE_SETTINGS_START(id + 1U));
        TEST_ASSERT_EQUAL_UINT32(EEPROM_PROFILE_MODULATION_START(id) +
                                     EEPROM_PROFILE_MODULATION_BLOCK_SIZE,
                                 EEPROM_PROFILE_MODULATION_START(id + 1U));
    }
}

void test_native_schema6_layout_accounts_for_slot_expansion() {
    constexpr size_t legacySlotBytes = sizeof(MIDISlot) - sizeof(SlotLfoConfig);
    constexpr Schema6StorageLayout layout =
        schema6StorageLayout(legacySlotBytes, 100, 200, 6);

    TEST_ASSERT_EQUAL_UINT32(sizeof(SlotLfoConfig) * NUM_SLOTS, layout.tailShift);
    TEST_ASSERT_EQUAL_UINT32(layout.oldConfigTail + layout.tailShift,
                             layout.newConfigTail);
    TEST_ASSERT_TRUE(layout.oldConfigTail < layout.newConfigTail);
}

void test_native_schema6_layout_places_macro_and_scene_tail_exactly() {
    constexpr size_t legacyMacroBytes = 913;
    constexpr size_t legacySceneBytes = 929;
    constexpr size_t sceneCount = 6;
    constexpr Schema6StorageLayout layout =
        schema6StorageLayout(sizeof(MIDISlot) - sizeof(SlotLfoConfig),
                             legacyMacroBytes, legacySceneBytes, sceneCount);

    TEST_ASSERT_EQUAL_UINT32(
        layout.oldConfigTail + NUM_PROFILES * EEPROM_PROFILE_BLOCK_SIZE,
        layout.oldProfileSettings);
    TEST_ASSERT_EQUAL_UINT32(
        layout.oldProfileSettings + NUM_PROFILES * EEPROM_PROFILE_SETTINGS_BLOCK_SIZE,
        layout.oldMacroStorage);
    TEST_ASSERT_EQUAL_UINT32(layout.oldMacroStorage + legacyMacroBytes,
                             layout.oldSceneStorage);
    TEST_ASSERT_EQUAL_UINT32(layout.oldSceneStorage + sceneCount * legacySceneBytes,
                             layout.oldRequiredStorage);
}

void test_native_schema7_layout_reserves_all_modulation_blocks() {
    constexpr Schema7StorageLayout layout = schema7StorageLayout();
    TEST_ASSERT_EQUAL_UINT32(EEPROM_PROFILE_MODULATION_BASE, layout.oldMacroStorage);
    TEST_ASSERT_EQUAL_UINT32(EEPROM_PROFILE_MODULATION_START(NUM_PROFILES),
                             layout.newMacroStorage);
    TEST_ASSERT_EQUAL_UINT32(NUM_PROFILES * EEPROM_PROFILE_MODULATION_BLOCK_SIZE,
                             layout.tailShift);
}

void test_native_profile_modulation_sanitizes_arg_and_lfo_payloads() {
    ProfileModulationExtension candidate{};
    candidate.version = 99;
    SlotARGConfig arg{};
    arg.enabled = 7;
    arg.method = static_cast<ARGMethod>(0xFF);
    arg.sourceA = 0xFF;
    arg.sourceB = 0xFF;
    candidate.slots[3].argPacked = packProfileSlotArg(arg);
    candidate.slots[3].lfo[0].flags = 0xFF;
    candidate.slots[3].lfo[0].amount = 127;
    candidate.slots[3].lfo[1].amount = -128;

    const ProfileModulationExtension sanitized = sanitizeProfileModulation(candidate);
    const SlotARGConfig restored = unpackProfileSlotArg(sanitized.slots[3].argPacked);
    TEST_ASSERT_EQUAL_UINT16(PROFILE_MODULATION_VERSION, sanitized.version);
    TEST_ASSERT_EQUAL_UINT8(1, restored.enabled);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ARGMethod::PLUS),
                            static_cast<uint8_t>(restored.method));
    TEST_ASSERT_EQUAL_UINT8(0, restored.sourceA);
    TEST_ASSERT_EQUAL_UINT8(1, restored.sourceB);
    TEST_ASSERT_EQUAL_INT8(100, sanitized.slots[3].lfo[0].amount);
    TEST_ASSERT_EQUAL_INT8(-100, sanitized.slots[3].lfo[1].amount);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ModCombineMode::Centered),
                            static_cast<uint8_t>(sanitized.slots[3].lfo[0].mode()));
}

void test_native_profile_modulation_crc_covers_semantic_slots_only() {
    ProfileModulationExtension extension{};
    extension.slots[7].argPacked = 0x0123;
    extension.slots[7].lfo[1].setEnabled(true);
    extension.slots[7].lfo[1].amount = -37;
    const uint16_t initial = computeProfileModulationCrc(extension);

    extension.version = 0xFFFF;
    extension.crc = 0x1234;
    TEST_ASSERT_EQUAL_UINT16(initial, computeProfileModulationCrc(extension));
    extension.slots[7].lfo[1].amount = -36;
    TEST_ASSERT_NOT_EQUAL(initial, computeProfileModulationCrc(extension));
}
