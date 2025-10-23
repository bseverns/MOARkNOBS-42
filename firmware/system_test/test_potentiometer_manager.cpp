#include <Arduino.h>
#include <unity.h>
#include "TestHelpers.h"

// PotentiometerManager owns the muxed pots that steer slots.  This quick
// check keeps the channel/CC setters from drifting.

// Stuff in a channel and CC number and make sure the getters echo them back.
void test_channel_and_cc() {
    PotentiometerManager pm = createPotentiometerManager();
    pm.setChannel(0, 9);
    pm.setCCNumber(0, 74);
    TEST_ASSERT_EQUAL_UINT8(9, pm.getChannel(0));
    TEST_ASSERT_EQUAL_UINT8(74, pm.getCCNumber(0));
}
