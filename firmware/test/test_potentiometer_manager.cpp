#include <unity.h>
#include "TestHelpers.h"

void test_channel_and_cc() {
    PotentiometerManager pm = createPotentiometerManager();
    pm.setChannel(0, 9);
    pm.setCCNumber(0, 74);
    TEST_ASSERT_EQUAL_UINT8(9, pm.getChannel(0));
    TEST_ASSERT_EQUAL_UINT8(74, pm.getCCNumber(0));
}

