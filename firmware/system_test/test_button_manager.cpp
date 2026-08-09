#define private public

#include <Arduino.h>
#include "Hardware/IO.h"
#include "Utility.h"
#include <vector>

// Mock time so we can step through the long-press dance
static unsigned long fakeMillis = 0;
unsigned long now() { return fakeMillis; }

#include "ButtonManager.h"
#undef private
#include "FirmwareState.h"
#include "LFO/LFOManager.h"
#include "TestHelpers.h"
#include "SystemTestShim.h"

// These system-level tests poke ButtonManager's private state machine directly
// to validate the long-press confirmation flow the hardware UI relies on.  We
// stub out millis() so we can fast-forward without waiting in real time.

namespace {
struct AnalogSequence {
    const int *values = nullptr;
    size_t size = 0;
    size_t index = 0;
} sequence;

int sequenceAnalog(uint8_t) {
    if (!sequence.values || sequence.size == 0) {
        return 1023;
    }
    int val = sequence.values[sequence.index % sequence.size];
    sequence.index++;
    return val;
}

class ScopedSequence {
  public:
    explicit ScopedSequence(const std::initializer_list<int> &vals)
        : guard_(sequenceAnalog), buffer_(vals) {
        sequence.values = buffer_.data();
        sequence.size = buffer_.size();
        sequence.index = 0;
    }

    void reset() { sequence.index = 0; }

    ~ScopedSequence() {
        sequence.values = nullptr;
        sequence.size = 0;
        sequence.index = 0;
    }

  private:
    hardware::ScopedAnalogReadProvider guard_;
    std::vector<int> buffer_;
};
} // namespace

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
                             led,   disp,      envs,     map,     diag,   diagPage,
                             profileRuntimeRequests};

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
                             led,   disp,      envs,     map,     diag,   diagPage,
                             profileRuntimeRequests};

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
                             led,   disp,      envs,     map,     diag,   diagPage,
                             profileRuntimeRequests};

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

void test_jitter_combo_updates_settings() {
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
                             led,   disp,      envs,     map,     diag,   diagPage,
                             profileRuntimeRequests};

    g_jitterSettings.depth = 0.0f;
    g_jitterSettings.smoothness = 1.0f;
    g_jitterTuningActive = false;

    const int pressed = 0;
    const int released = 1023;
    bm.buttonStates[NUM_VIRTUAL_BUTTONS + 0] = true;
    bm.buttonStates[NUM_VIRTUAL_BUTTONS + 1] = false;
    bm.buttonStates[NUM_VIRTUAL_BUTTONS + 2] = false;
    bm.buttonStates[NUM_VIRTUAL_BUTTONS + 3] = true;
    bm.buttonStates[NUM_VIRTUAL_BUTTONS + 4] = true;
    bm.buttonStates[NUM_VIRTUAL_BUTTONS + 5] = false;
    ScopedSequence values{
        pressed, released, released, pressed, pressed, released, // ctrl 0..5
        1023,    0,        512                                   // control pots
    };

    fakeMillis = DEBOUNCE_DELAY + 1;
    bm.scanControlInputs(ctx);

    TEST_ASSERT_TRUE(g_jitterTuningActive);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.0f, g_jitterSettings.depth);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, g_jitterSettings.smoothness);
}

void test_config_mode_combo_autosaves_dirty_changes() {
    auto pm = createPotentiometerManager();
    auto bm = createButtonManager(&pm);

    auto cfg = createConfigManager();
    auto led = createLEDManager();
    auto disp = createDisplayManager();
    auto envs = createEnvelopeFollowers(&pm);
    std::vector<uint8_t> potCh(NUM_POTS, 0);
    uint8_t activePot = 0;
    uint8_t activeCh = 1;
    bool envMode = false;
    const char *envStr = "";
    std::map<int, MIDISlot::EfSettings> map;
    bool diag = false;
    uint8_t diagPage = 0;
    ButtonManagerContext ctx{potCh, activePot, activeCh, envMode, envStr, cfg,
                             led,   disp,      envs,     map,     diag,   diagPage,
                             profileRuntimeRequests};

    profileRuntimeRequests.clear();
    const uint8_t ccBefore = cfg.getPotCCNumber(activePot);
    constexpr uint8_t kConfigMask =
        static_cast<uint8_t>((1u << 0) | (1u << 2) | (1u << 3) | (1u << 5));

    bm.handleMultiButtonPress(kConfigMask, ctx);
    TEST_ASSERT_TRUE(bm.isOnDeviceConfigModeActive());
    TEST_ASSERT_FALSE(profileRuntimeRequests.savePending());

    bm.handleSingleButtonPress(NUM_VIRTUAL_BUTTONS + 4, ctx); // change CC/data1 + dirty
    TEST_ASSERT_TRUE(bm._onDeviceConfigModeDirty);
    TEST_ASSERT_TRUE(ccBefore != cfg.getPotCCNumber(activePot));

    bm.handleSingleButtonPress(NUM_VIRTUAL_BUTTONS + 5, ctx); // exit + autosave
    TEST_ASSERT_FALSE(bm.isOnDeviceConfigModeActive());
    TEST_ASSERT_FALSE(bm._onDeviceConfigModeDirty);
    TEST_ASSERT_TRUE(profileRuntimeRequests.savePending());
}

void test_clock_source_combo_toggles_follow_external() {
    auto pm = createPotentiometerManager();
    auto bm = createButtonManager(&pm);

    auto cfg = createConfigManager();
    auto led = createLEDManager();
    auto disp = createDisplayManager();
    auto envs = createEnvelopeFollowers(&pm);
    std::vector<uint8_t> potCh(NUM_POTS, 0);
    uint8_t activePot = 0;
    uint8_t activeCh = 1;
    bool envMode = false;
    const char *envStr = "";
    std::map<int, MIDISlot::EfSettings> map;
    bool diag = false;
    uint8_t diagPage = 0;
    ButtonManagerContext ctx{potCh, activePot, activeCh, envMode, envStr, cfg,
                             led,   disp,      envs,     map,     diag,   diagPage,
                             profileRuntimeRequests};

    constexpr uint8_t kClockMask = static_cast<uint8_t>((1u << 1) | (1u << 4) | (1u << 5));
    const bool startFollow = g_followExternalClock;

    bm.handleMultiButtonPress(kClockMask, ctx);
    TEST_ASSERT_EQUAL(!startFollow, g_followExternalClock);

    bm.handleMultiButtonPress(kClockMask, ctx);
    TEST_ASSERT_EQUAL(startFollow, g_followExternalClock);
}

void test_lfo_tuning_combo_and_route_cycle() {
    auto pm = createPotentiometerManager();
    auto bm = createButtonManager(&pm);

    auto cfg = createConfigManager();
    auto led = createLEDManager();
    auto disp = createDisplayManager();
    auto envs = createEnvelopeFollowers(&pm);
    std::vector<uint8_t> potCh(NUM_POTS, 0);
    uint8_t activePot = 0;
    uint8_t activeCh = 1;
    bool envMode = false;
    const char *envStr = "";
    std::map<int, MIDISlot::EfSettings> map;
    bool diag = false;
    uint8_t diagPage = 0;
    ButtonManagerContext ctx{potCh, activePot, activeCh, envMode, envStr, cfg,
                             led,   disp,      envs,     map,     diag,   diagPage,
                             profileRuntimeRequests};

    constexpr uint8_t kLfoMask = static_cast<uint8_t>((1u << 0) | (1u << 1) | (1u << 3));

    lfoManager.clearRoutes();
    lfoManager.lfo(0).setShape(LFOShape::Sine);
    lfoManager.lfo(0).setSyncEnabled(false);
    lfoManager.lfo(0).setFrequencyHz(1.0f);
    lfoManager.lfo(0).setDepth(0.5f);

    bm.handleMultiButtonPress(kLfoMask, ctx);
    TEST_ASSERT_TRUE(bm.isLfoTuningModeActive());
    TEST_ASSERT_EQUAL_UINT8(0, bm.lfoTuningIndex());

    bm.handleSingleButtonPress(NUM_VIRTUAL_BUTTONS + 4, ctx); // add internal route
    TEST_ASSERT_EQUAL_UINT(1, lfoManager.routeCount());
    LFOManager::Route route{};
    TEST_ASSERT_TRUE(lfoManager.getRoute(0, route));
    TEST_ASSERT_EQUAL_UINT8(0, route.lfoIndex);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(LFOInternalTarget::EfGainTrim),
                            static_cast<uint8_t>(route.target));

    bm.handleSingleButtonPress(NUM_VIRTUAL_BUTTONS + 4, ctx); // cycle target
    TEST_ASSERT_TRUE(lfoManager.getRoute(0, route));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(LFOInternalTarget::ArpSwing),
                            static_cast<uint8_t>(route.target));

    bm.handleSingleButtonPress(NUM_VIRTUAL_BUTTONS + 1, ctx); // LFO2 select
    TEST_ASSERT_EQUAL_UINT8(1, bm.lfoTuningIndex());

    bm.handleSingleButtonPress(NUM_VIRTUAL_BUTTONS + 5, ctx); // exit
    TEST_ASSERT_FALSE(bm.isLfoTuningModeActive());
}
