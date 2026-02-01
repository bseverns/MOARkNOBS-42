#include <Arduino.h>
#include "SystemTestShim.h"
#include "EnvelopeFollower.h"
#include "PotentiometerManager.h"
#include "TestHelpers.h"
#include "Globals.h"

// The envelope follower owns the LED vibe, so we sanity-check that flipping
// between filter modes actually changes how a steady signal gets treated.

// The envelope follower lives and dies by its filter settings.
// This test flips between low-pass and high-pass modes and ensures
// that DC signals get squashed when they should.

// Pump a constant into LOWPASS then HIGHPASS and confirm the former hugs the
// input while the latter crushes it toward zero.
void test_filter_type_switching() {
    auto pm = createPotentiometerManager();
    EnvelopeFollower env(A0, &pm, 0);

    env.setFilterType(EnvelopeFollower::LOWPASS);
    int lp = 0;
    for (int i = 0; i < 10; ++i) {
        lp = env.processEnvelopeLevel(100); // steady DC level
    }

    env.setFilterType(EnvelopeFollower::HIGHPASS);
    int hp = 0;
    for (int i = 0; i < 10; ++i) {
        hp = env.processEnvelopeLevel(100); // should bleed out to near zero
    }

    // low-pass should pass the DC level mostly unchanged
    TEST_ASSERT_INT_WITHIN(10, 100, lp);
    // high-pass should murder DC
    TEST_ASSERT_LESS_THAN(10, hp);
}

void test_random_mode_respects_jitter_depth() {
    auto pm = createPotentiometerManager();
    EnvelopeFollower env(A0, &pm, 0);

    g_jitterSettings.depth = 0.0f;
    g_jitterSettings.smoothness = 0.5f;

    env.setFilterType(EnvelopeFollower::RANDOM);
    env.configureFilter(5000.0f, 4.0f);

    for (int i = 0; i < 5; ++i) {
        int out = env.processEnvelopeLevel(90);
        TEST_ASSERT_EQUAL_INT(90, out);
    }
}
