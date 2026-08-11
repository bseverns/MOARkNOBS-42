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
    input.lfoNormalized = {0.45f, 0.52f};
    input.lfoSigned = {-10.0f / 127.0f, 5.0f / 127.0f};
    for (SlotLfoLane &lane : input.lfoLane) {
        lane.setEnabled(true);
        lane.amount = 100;
    }

    const SlotModulationResult result = resolveSlotModulationWithContributions(input);
    TEST_ASSERT_EQUAL_UINT8(67, result.finalValue);
    TEST_ASSERT_EQUAL_UINT8(50, result.baseline);
    TEST_ASSERT_EQUAL_INT16(20, result.efDelta);
    TEST_ASSERT_EQUAL_INT16(-5, result.lfoDelta[0]);
    TEST_ASSERT_EQUAL_INT16(2, result.lfoDelta[1]);
    TEST_ASSERT_TRUE(result.efApplied);
    TEST_ASSERT_TRUE(result.lfoApplied[0]);
    TEST_ASSERT_TRUE(result.lfoApplied[1]);
    TEST_ASSERT_EQUAL_INT16(
        result.finalValue,
        result.baseline + result.efDelta + result.lfoDelta[0] + result.lfoDelta[1]);
}

void test_slot_modulation_supports_lfo_without_ef() {
    SlotModulationInput input{};
    input.baseline = 61;
    input.lfoActive[0] = true;
    input.lfoNormalized[0] = 0.55f;
    input.lfoSigned[0] = 12.0f / 127.0f;
    input.lfoLane[0].setEnabled(true);
    input.lfoLane[0].amount = 100;

    TEST_ASSERT_EQUAL_UINT8(67, resolveSlotModulation(input));
}

void test_slot_modulation_clamps_after_each_lane() {
    SlotModulationInput input{};
    input.baseline = 120;
    input.efActive = true;
    input.efValue = 20;
    input.efMode = EfDestinationMode::AddClamp;
    input.lfoActive[0] = true;
    input.lfoNormalized[0] = 0.25f;
    input.lfoSigned[0] = -64.0f / 127.0f;
    input.lfoLane[0].setEnabled(true);
    input.lfoLane[0].amount = 100;

    const SlotModulationResult result = resolveSlotModulationWithContributions(input);
    TEST_ASSERT_EQUAL_UINT8(95, result.finalValue);
    TEST_ASSERT_EQUAL_INT16(7, result.efDelta);
    TEST_ASSERT_EQUAL_INT16(-32, result.lfoDelta[0]);
}

void test_slot_modulation_honors_ef_replace_before_lfo() {
    SlotModulationInput input{};
    input.baseline = 10;
    input.efActive = true;
    input.efValue = 90;
    input.efMode = EfDestinationMode::Replace;
    input.lfoActive[1] = true;
    input.lfoNormalized[1] = 0.42f;
    input.lfoSigned[1] = -20.0f / 127.0f;
    input.lfoLane[1].setEnabled(true);
    input.lfoLane[1].amount = 100;

    TEST_ASSERT_EQUAL_UINT8(80, resolveSlotModulation(input));
}

void test_slot_modulation_applies_replace_and_scale_lane_modes() {
    SlotModulationInput input{};
    input.baseline = 80;
    input.lfoActive[0] = true;
    input.lfoNormalized[0] = 0.75f;
    input.lfoSigned[0] = 0.5f;
    input.lfoLane[0].setEnabled(true);
    input.lfoLane[0].setMode(ModCombineMode::Replace);
    input.lfoLane[0].amount = 50;
    input.lfoActive[1] = true;
    input.lfoNormalized[1] = 0.25f;
    input.lfoSigned[1] = -0.5f;
    input.lfoLane[1].setEnabled(true);
    input.lfoLane[1].setMode(ModCombineMode::Scale);
    input.lfoLane[1].amount = 50;

    // Replace produces 80, then Scale applies 0.75.
    const SlotModulationResult result = resolveSlotModulationWithContributions(input);
    TEST_ASSERT_EQUAL_UINT8(60, result.finalValue);
    TEST_ASSERT_EQUAL_INT16(0, result.lfoDelta[0]);
    TEST_ASSERT_EQUAL_INT16(-20, result.lfoDelta[1]);
}

void test_slot_modulation_distinguishes_unipolar_add_from_centered() {
    SlotModulationInput input{};
    input.baseline = 20;
    input.lfoActive[0] = true;
    input.lfoNormalized[0] = 0.75f;
    input.lfoSigned[0] = 0.5f;
    input.lfoLane[0].setEnabled(true);
    input.lfoLane[0].amount = 100;

    input.lfoLane[0].setMode(ModCombineMode::AddClamp);
    TEST_ASSERT_EQUAL_UINT8(115, resolveSlotModulation(input));

    input.lfoLane[0].setMode(ModCombineMode::Centered);
    TEST_ASSERT_EQUAL_UINT8(52, resolveSlotModulation(input));
}

void test_slot_modulation_centered_full_amount_spans_midi_range() {
    SlotModulationInput input{};
    input.baseline = 64;
    input.lfoActive[0] = true;
    input.lfoLane[0].setEnabled(true);
    input.lfoLane[0].setMode(ModCombineMode::Centered);
    input.lfoLane[0].amount = 100;

    input.lfoSigned[0] = -1.0f;
    TEST_ASSERT_EQUAL_UINT8(0, resolveSlotModulation(input));
    input.lfoSigned[0] = 1.0f;
    TEST_ASSERT_EQUAL_UINT8(127, resolveSlotModulation(input));
}

void test_slot_modulation_legacy_replace_preserves_output_curve() {
    constexpr uint8_t legacyValues[] = {0, 1, 31, 63, 64, 65, 96, 126, 127};
    constexpr uint8_t baselines[] = {0, 37, 64, 101, 127};
    for (uint8_t baseline : baselines) {
        for (uint8_t legacyValue : legacyValues) {
            SlotModulationInput input{};
            input.baseline = baseline;
            input.lfoActive[0] = true;
            input.lfoNormalized[0] = static_cast<float>(legacyValue) / 127.0f;
            input.lfoSigned[0] = signedSlotLfoFromMidiValue(legacyValue);
            input.lfoLane[0].setEnabled(true);
            input.lfoLane[0].setMode(ModCombineMode::Replace);
            input.lfoLane[0].amount = 100;
            TEST_ASSERT_EQUAL_UINT8(legacyValue, resolveSlotModulation(input));
        }
    }
}
