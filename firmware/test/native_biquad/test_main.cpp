#include <unity.h>

void test_lowpass_preserves_dc_on_native_host();
void test_highpass_rejects_dc_on_native_host();
void test_native_filter_clamps_extreme_cutoffs();

extern "C" void setUp() {}
extern "C" void tearDown() {}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_lowpass_preserves_dc_on_native_host);
    RUN_TEST(test_highpass_rejects_dc_on_native_host);
    RUN_TEST(test_native_filter_clamps_extreme_cutoffs);
    return UNITY_END();
}
