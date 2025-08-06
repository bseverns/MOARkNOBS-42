#include <unity.h>
#include "TestHelpers.h"

void test_update_interval_round_trip() {
    DisplayManager dm = createDisplayManager();
    dm.setUpdateInterval(1234);
    TEST_ASSERT_EQUAL_UINT32(1234, dm.getUpdateInterval());
}

void setUp(void) {}
void tearDown(void) {}

void setup() {
    UNITY_BEGIN();
    RUN_TEST(test_update_interval_round_trip);
    UNITY_END();
}

void loop() {}
