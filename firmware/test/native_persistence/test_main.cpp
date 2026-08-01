#include <unity.h>

void test_native_storage_regions_are_contiguous_and_non_overlapping();
void test_native_schema6_layout_accounts_for_slot_expansion();
void test_native_schema6_layout_places_macro_and_scene_tail_exactly();
void test_native_schema7_layout_reserves_all_modulation_blocks();
void test_native_profile_modulation_sanitizes_arg_and_lfo_payloads();
void test_native_profile_modulation_crc_covers_semantic_slots_only();

extern "C" void setUp() {}
extern "C" void tearDown() {}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_native_storage_regions_are_contiguous_and_non_overlapping);
    RUN_TEST(test_native_schema6_layout_accounts_for_slot_expansion);
    RUN_TEST(test_native_schema6_layout_places_macro_and_scene_tail_exactly);
    RUN_TEST(test_native_schema7_layout_reserves_all_modulation_blocks);
    RUN_TEST(test_native_profile_modulation_sanitizes_arg_and_lfo_payloads);
    RUN_TEST(test_native_profile_modulation_crc_covers_semantic_slots_only);
    return UNITY_END();
}
