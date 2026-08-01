#include <unity.h>

void test_native_modulation_composes_sources_in_contract_order();
void test_native_modulation_supports_every_lfo_mode();
void test_native_modulation_clamps_inputs_and_each_stage();
void test_native_modulation_legacy_replace_curve_is_exact();
void test_native_modulation_sanitizes_persisted_lane_bits_and_amounts();

extern "C" void setUp() {}
extern "C" void tearDown() {}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_native_modulation_composes_sources_in_contract_order);
    RUN_TEST(test_native_modulation_supports_every_lfo_mode);
    RUN_TEST(test_native_modulation_clamps_inputs_and_each_stage);
    RUN_TEST(test_native_modulation_legacy_replace_curve_is_exact);
    RUN_TEST(test_native_modulation_sanitizes_persisted_lane_bits_and_amounts);
    return UNITY_END();
}
