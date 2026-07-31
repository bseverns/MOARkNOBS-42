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
    input.lfoValue = {-10.0f / 127.0f, 5.0f / 127.0f};
    for (SlotLfoLane &lane : input.lfoLane) {
        lane.setEnabled(true);
        lane.amount = 100;
    }

    TEST_ASSERT_EQUAL_UINT8(65, resolveSlotModulation(input));
}

void test_slot_modulation_supports_lfo_without_ef() {
    SlotModulationInput input{};
    input.baseline = 61;
    input.lfoActive[0] = true;
    input.lfoValue[0] = 12.0f / 127.0f;
    input.lfoLane[0].setEnabled(true);
    input.lfoLane[0].amount = 100;

    TEST_ASSERT_EQUAL_UINT8(73, resolveSlotModulation(input));
}

void test_slot_modulation_clamps_after_each_lane() {
    SlotModulationInput input{};
    input.baseline = 120;
    input.efActive = true;
    input.efValue = 20;
    input.efMode = EfDestinationMode::AddClamp;
    input.lfoActive[0] = true;
    input.lfoValue[0] = -64.0f / 127.0f;
    input.lfoLane[0].setEnabled(true);
    input.lfoLane[0].amount = 100;

    TEST_ASSERT_EQUAL_UINT8(63, resolveSlotModulation(input));
}

void test_slot_modulation_honors_ef_replace_before_lfo() {
    SlotModulationInput input{};
    input.baseline = 10;
    input.efActive = true;
    input.efValue = 90;
    input.efMode = EfDestinationMode::Replace;
    input.lfoActive[1] = true;
    input.lfoValue[1] = -20.0f / 127.0f;
    input.lfoLane[1].setEnabled(true);
    input.lfoLane[1].amount = 100;

    TEST_ASSERT_EQUAL_UINT8(70, resolveSlotModulation(input));
}

void test_slot_modulation_applies_replace_and_scale_lane_modes() {
    SlotModulationInput input{};
    input.baseline = 80;
    input.lfoActive[0] = true;
    input.lfoValue[0] = 0.5f;
    input.lfoLane[0].setEnabled(true);
    input.lfoLane[0].setMode(ModCombineMode::Replace);
    input.lfoLane[0].amount = 50;
    input.lfoActive[1] = true;
    input.lfoValue[1] = -0.5f;
    input.lfoLane[1].setEnabled(true);
    input.lfoLane[1].setMode(ModCombineMode::Scale);
    input.lfoLane[1].amount = 50;

    // Replace produces 80, then Scale applies 0.75.
    TEST_ASSERT_EQUAL_UINT8(60, resolveSlotModulation(input));
}
