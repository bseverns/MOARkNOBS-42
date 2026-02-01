#include <Arduino.h>
#include "SystemTestShim.h"
#include "TestHelpers.h"

// DisplayManager's system test is lightweight—just make sure whatever cadence
// we program survives a round trip so the manual smoke tests stay predictable.

// Set an arbitrary interval, pull it back out, and confirm nothing garbles the
// stored value.
void test_update_interval_round_trip() {
    DisplayManager dm = createDisplayManager();
    dm.setUpdateInterval(1234);
    TEST_ASSERT_EQUAL_UINT32(1234, dm.getUpdateInterval());
}
