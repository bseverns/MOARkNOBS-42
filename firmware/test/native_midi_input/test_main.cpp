#include <unity.h>

void test_router_matches_port_channel_and_controller();
void test_router_soft_pickup_waits_for_crossing();
void test_router_soft_pickup_rearms_after_external_target_move();
void test_router_toggle_fires_only_on_rising_edges();
void test_router_toggle_retries_same_value_after_failed_apply();
void test_router_supports_multiple_destinations_for_one_source();

extern "C" void setUp() {}
extern "C" void tearDown() {}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_router_matches_port_channel_and_controller);
    RUN_TEST(test_router_soft_pickup_waits_for_crossing);
    RUN_TEST(test_router_soft_pickup_rearms_after_external_target_move);
    RUN_TEST(test_router_toggle_fires_only_on_rising_edges);
    RUN_TEST(test_router_toggle_retries_same_value_after_failed_apply);
    RUN_TEST(test_router_supports_multiple_destinations_for_one_source);
    return UNITY_END();
}
