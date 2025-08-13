#include "unity_config.h"  // yanks in Arduino and our usbMIDI doppelganger
#include <unity.h>
#include "Arpeggiator.h"

// Unity expects these, even if they just wave from the sidelines.
void setUp() {}
void tearDown() {}

void test_start_stop_cycle() {
    Arpeggiator arp;
    TEST_ASSERT_FALSE(arp.isActive());
    arp.start(7);
    TEST_ASSERT_TRUE(arp.isActive());
    TEST_ASSERT_EQUAL_UINT8(7, arp.getSlot());
    arp.stop();
    TEST_ASSERT_FALSE(arp.isActive());
}

