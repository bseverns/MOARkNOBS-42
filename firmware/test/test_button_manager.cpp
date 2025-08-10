#define private public

// Mock time so we can step through the long-press dance
static unsigned long fakeMillis = 0;
#define millis() (fakeMillis)

#include "ButtonManager.h"
#undef millis
#undef private
#include "TestHelpers.h"
#include <unity.h>

void test_long_press_detection() {
    auto pm = createPotentiometerManager();
    ButtonManager bm(primaryMuxPins, secondaryMuxPins, buttonMuxAnalogPin, TEST_CONTROL_PINS, &pm);

    auto cfg = createConfigManager();
    auto led = createLEDManager();
    auto disp = createDisplayManager();
    auto envs = createEnvelopeFollowers(&pm);
    std::vector<uint8_t> potCh(NUM_POTS, 0);
    uint8_t activePot = 0;
    uint8_t activeCh = 0;
    bool envMode = false;
    const char* envStr = "";
    std::map<int,int> map;
    ButtonManagerContext ctx{potCh, activePot, activeCh, envMode, envStr, cfg, led, disp, envs, map};

    fakeMillis = 0;
    bm.updateButtonStateMachine(0, true, ctx); // press
    fakeMillis = 499; // just shy of the 500 ms threshold
    bm.updateButtonStateMachine(0, true, ctx);
    TEST_ASSERT_EQUAL(ButtonState::PRESSED, bm._buttonMachines[0].state);
    TEST_ASSERT_FALSE(bm._buttonMachines[0].longPressFired);

    fakeMillis = 501; // crossed the line
    bm.updateButtonStateMachine(0, true, ctx);
    TEST_ASSERT_EQUAL(ButtonState::LONG_PRESS, bm._buttonMachines[0].state);
    TEST_ASSERT_TRUE(bm._buttonMachines[0].longPressFired);
}

void setUp(void) {}
void tearDown(void) {}

void setup() {
    UNITY_BEGIN();
    RUN_TEST(test_long_press_detection);
    UNITY_END();
}

void loop() {}
