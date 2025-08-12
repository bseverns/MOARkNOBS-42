#include <unity.h>
#include "TestHelpers.h"

void test_update_interval_round_trip() {
    DisplayManager dm = createDisplayManager();
    dm.setUpdateInterval(1234);
    TEST_ASSERT_EQUAL_UINT32(1234, dm.getUpdateInterval());
}

