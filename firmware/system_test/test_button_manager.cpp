#define private public

#include <Arduino.h>

// Mock time so we can step through the long-press dance
static unsigned long fakeMillis = 0;
unsigned long now() { return fakeMillis; }

#include "ButtonManager.h"
#undef private
#include "TestHelpers.h"
#include <unity.h>

// These system-level tests poke ButtonManager's private state machine directly
// to validate the long-press confirmation flow the hardware UI relies on.  We
// stub out millis() so we can fast-forward without waiting in real time.

// Hold the button past the 500 ms mark and make sure we enter the long-press
// state exactly once.
void test_long_press_detection() {
    auto pm = createPotentiometerManager();
    auto bm = createButtonManager(&pm);

    auto cfg = createConfigManager();
    auto led = createLEDManager();
    auto disp = createDisplayManager();
    auto envs = createEnvelopeFollowers(&pm);
    std::vector<uint8_t> potCh(NUM_POTS, 0);
    uint8_t activePot = 0;
    uint8_t activeCh = 0;
    bool envMode = false;
    const char *envStr = "";
    std::map<int, MIDISlot::EfSettings> map;
    bool diag = false;
    uint8_t diagPage = 0;
    ButtonManagerContext ctx{potCh, activePot, activeCh, envMode, envStr, cfg,
                             led,   disp,      envs,     map,     diag,   diagPage};

    fakeMillis = 0;
    bm.updateButtonStateMachine(0, true, ctx); // press
    fakeMillis = 499;                          // just shy of the 500 ms threshold
    bm.updateButtonStateMachine(0, true, ctx);
    TEST_ASSERT_EQUAL(ButtonState::PRESSED, bm._buttonMachines[0].state);
    TEST_ASSERT_FALSE(bm._buttonMachines[0].longPressFired);

    fakeMillis = 501; // crossed the line
    bm.updateButtonStateMachine(0, true, ctx);
    TEST_ASSERT_EQUAL(ButtonState::LONG_PRESS, bm._buttonMachines[0].state);
    TEST_ASSERT_TRUE(bm._buttonMachines[0].longPressFired);
}

// A long-press shouldn't trigger the destructive action until the user taps a
// confirmation button.  This run mimics that two-step handshake.
void test_long_press_requires_confirm() {
    auto pm = createPotentiometerManager();
    auto bm = createButtonManager(&pm);

    auto cfg = createConfigManager();
    auto led = createLEDManager();
    auto disp = createDisplayManager();
    auto envs = createEnvelopeFollowers(&pm);
    std::vector<uint8_t> potCh(NUM_POTS, 0);
    uint8_t activePot = 0;
    uint8_t activeCh = 0;
    bool envMode = false;
    const char *envStr = "";
    std::map<int, MIDISlot::EfSettings> map;
    bool diag = false;
    uint8_t diagPage = 0;
    ButtonManagerContext ctx{potCh, activePot, activeCh, envMode, envStr, cfg,
                             led,   disp,      envs,     map,     diag,   diagPage};

    // Long press triggers confirm but no action yet
    fakeMillis = 0;
    bm.updateButtonStateMachine(0, true, ctx); // press
    fakeMillis = 600;
    bm.updateButtonStateMachine(0, true, ctx);  // hold past threshold
    bm.updateButtonStateMachine(0, false, ctx); // release
    TEST_ASSERT_EQUAL(0, bm._confirmIndex);
    TEST_ASSERT_TRUE(ctx.potToEnvelopeMap.empty());

    // Second tap within window commits the move
    fakeMillis = 800;
    bm.updateButtonStateMachine(0, true, ctx);
    bm.updateButtonStateMachine(0, false, ctx);
    TEST_ASSERT_EQUAL(-1, bm._confirmIndex);
    TEST_ASSERT_FALSE(ctx.potToEnvelopeMap.empty());
}

void test_double_press_ctrl2_cycles_midi_type() {
    auto pm = createPotentiometerManager();
    auto bm = createButtonManager(&pm);

    auto cfg = createConfigManager();
    auto led = createLEDManager();
    auto disp = createDisplayManager();
    auto envs = createEnvelopeFollowers(&pm);
    std::vector<uint8_t> potCh(NUM_POTS, 0);
    uint8_t activePot = 0;
    uint8_t activeCh = 0;
    bool envMode = false;
    const char *envStr = "";
    std::map<int, MIDISlot::EfSettings> map;
    bool diag = false;
    uint8_t diagPage = 0;
    ButtonManagerContext ctx{potCh, activePot, activeCh, envMode, envStr, cfg,
                             led,   disp,      envs,     map,     diag,   diagPage};

    MIDISlot &slot = cfg.getSlot(0);
    slot.type = MIDIMessageType::CC;

    uint8_t ctrlIdx = NUM_VIRTUAL_BUTTONS + 2;
    fakeMillis = 0;
    bm.updateButtonStateMachine(ctrlIdx, true, ctx);
    fakeMillis = 50;
    bm.updateButtonStateMachine(ctrlIdx, false, ctx);
    fakeMillis = 100;
    bm.updateButtonStateMachine(ctrlIdx, true, ctx);
    fakeMillis = 150;
    bm.updateButtonStateMachine(ctrlIdx, false, ctx);

    TEST_ASSERT_EQUAL(MIDIMessageType::Note, slot.type);
}
