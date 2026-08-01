#include <unity.h>

#include "SlotModulationResolver.h"

void test_native_modulation_composes_sources_in_contract_order() {
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

    TEST_ASSERT_EQUAL_UINT8(67, resolveSlotModulation(input));
}

void test_native_modulation_supports_every_lfo_mode() {
    SlotModulationInput input{};
    input.baseline = 64;
    input.lfoActive[0] = true;
    input.lfoNormalized[0] = 0.5f;
    input.lfoSigned[0] = 0.5f;
    input.lfoLane[0].setEnabled(true);
    input.lfoLane[0].amount = 50;

    input.lfoLane[0].setMode(ModCombineMode::AddClamp);
    TEST_ASSERT_EQUAL_UINT8(96, resolveSlotModulation(input));
    input.lfoLane[0].setMode(ModCombineMode::Subtract);
    TEST_ASSERT_EQUAL_UINT8(32, resolveSlotModulation(input));
    input.lfoLane[0].setMode(ModCombineMode::Centered);
    TEST_ASSERT_EQUAL_UINT8(80, resolveSlotModulation(input));
    input.lfoLane[0].setMode(ModCombineMode::Replace);
    TEST_ASSERT_EQUAL_UINT8(80, resolveSlotModulation(input));
    input.lfoLane[0].setMode(ModCombineMode::Scale);
    TEST_ASSERT_EQUAL_UINT8(80, resolveSlotModulation(input));
}

void test_native_modulation_clamps_inputs_and_each_stage() {
    SlotModulationInput input{};
    input.baseline = 120;
    input.efActive = true;
    input.efValue = 30;
    input.efMode = EfDestinationMode::AddClamp;
    input.lfoActive[0] = true;
    input.lfoNormalized[0] = 2.0f;
    input.lfoSigned[0] = -2.0f;
    input.lfoLane[0].setEnabled(true);
    input.lfoLane[0].setMode(ModCombineMode::Centered);
    input.lfoLane[0].amount = 100;

    TEST_ASSERT_EQUAL_UINT8(63, resolveSlotModulation(input));
}

void test_native_modulation_legacy_replace_curve_is_exact() {
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

void test_native_modulation_sanitizes_persisted_lane_bits_and_amounts() {
    SlotLfoLane lane{};
    lane.flags = 0xFF;
    lane.amount = 127;
    lane = sanitizeSlotLfoLane(lane);
    TEST_ASSERT_TRUE(lane.enabled());
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ModCombineMode::Centered),
                            static_cast<uint8_t>(lane.mode()));
    TEST_ASSERT_EQUAL_INT8(100, lane.amount);

    lane.flags = 0;
    lane.amount = -128;
    lane = sanitizeSlotLfoLane(lane);
    TEST_ASSERT_FALSE(lane.enabled());
    TEST_ASSERT_EQUAL_INT8(-100, lane.amount);
}
