#include <unity.h>

void test_transport_reset_uses_current_clock_and_initial_budget();
void test_transport_refill_caps_and_handles_clock_wrap();
void test_transport_prioritizes_notes_and_rotates_fairly();
void test_transport_failed_attempt_preserves_tokens();

extern "C" void setUp() {}
extern "C" void tearDown() {}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_transport_reset_uses_current_clock_and_initial_budget);
    RUN_TEST(test_transport_refill_caps_and_handles_clock_wrap);
    RUN_TEST(test_transport_prioritizes_notes_and_rotates_fairly);
    RUN_TEST(test_transport_failed_attempt_preserves_tokens);
    return UNITY_END();
}
