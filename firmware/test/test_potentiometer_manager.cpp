#include <unity.h>
#include "TestHelpers.h"

void test_channel_and_cc() {
    PotentiometerManager pm = createPotentiometerManager();
    pm.setChannel(0, 9);
    pm.setCCNumber(0, 74);
    TEST_ASSERT_EQUAL_UINT8(9, pm.getChannel(0));
    TEST_ASSERT_EQUAL_UINT8(74, pm.getCCNumber(0));
}

void setUp(void) {}
void tearDown(void) {}

void setup() {
    UNITY_BEGIN();
    RUN_TEST(test_channel_and_cc);
    UNITY_END();
}

void loop() {}
