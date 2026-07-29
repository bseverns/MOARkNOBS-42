#include <unity.h>

void test_lowpass_preserves_dc_on_native_host();
void test_highpass_rejects_dc_on_native_host();
void test_native_filter_clamps_extreme_cutoffs();
void test_follower_step_response_uses_production_control_cadence();

extern "C" void setUp() {}
extern "C" void tearDown() {}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_lowpass_preserves_dc_on_native_host);
    RUN_TEST(test_highpass_rejects_dc_on_native_host);
    RUN_TEST(test_native_filter_clamps_extreme_cutoffs);
    RUN_TEST(test_follower_step_response_uses_production_control_cadence);
    return UNITY_END();
}
