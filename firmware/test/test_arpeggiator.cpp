#include <unity.h>
#include "Arpeggiator.h"

void test_start_stop_cycle() {
    Arpeggiator arp;
    TEST_ASSERT_FALSE(arp.isActive());
    arp.start(7);
    TEST_ASSERT_TRUE(arp.isActive());
    TEST_ASSERT_EQUAL_UINT8(7, arp.getSlot());
    arp.stop();
    TEST_ASSERT_FALSE(arp.isActive());
}

void setUp(void) {}
void tearDown(void) {}

void setup() {
    UNITY_BEGIN();
    RUN_TEST(test_start_stop_cycle);
    UNITY_END();
}

void loop() {}
