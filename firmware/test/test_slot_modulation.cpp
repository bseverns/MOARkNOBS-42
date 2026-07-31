#include "unity_config.h"
#include <unity.h>

#include "SlotModulationResolver.h"

void test_slot_modulation_composes_ef_then_lfos() {
    SlotModulationInput input{};
    input.baseline = 50;
    input.efActive = true;
    input.efValue = 20;
    input.efMode = EfDestinationMode::AddClamp;
    input.lfoActive = {true, true};
    input.lfoValue = {54, 69}; // -10, then +5

    TEST_ASSERT_EQUAL_UINT8(65, resolveSlotModulation(input));
}

void test_slot_modulation_supports_lfo_without_ef() {
    SlotModulationInput input{};
    input.baseline = 61;
    input.lfoActive[0] = true;
    input.lfoValue[0] = 76;

    TEST_ASSERT_EQUAL_UINT8(73, resolveSlotModulation(input));
}

void test_slot_modulation_clamps_after_each_lane() {
    SlotModulationInput input{};
    input.baseline = 120;
    input.efActive = true;
    input.efValue = 20;
    input.efMode = EfDestinationMode::AddClamp;
    input.lfoActive[0] = true;
    input.lfoValue[0] = 0;

    TEST_ASSERT_EQUAL_UINT8(63, resolveSlotModulation(input));
}

void test_slot_modulation_honors_ef_replace_before_lfo() {
    SlotModulationInput input{};
    input.baseline = 10;
    input.efActive = true;
    input.efValue = 90;
    input.efMode = EfDestinationMode::Replace;
    input.lfoActive[1] = true;
    input.lfoValue[1] = 44;

    TEST_ASSERT_EQUAL_UINT8(70, resolveSlotModulation(input));
}
