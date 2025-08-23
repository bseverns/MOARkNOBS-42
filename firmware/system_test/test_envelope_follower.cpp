#include <Arduino.h>
#include <unity.h>
#include "EnvelopeFollower.h"
#include "PotentiometerManager.h"
#include "TestHelpers.h"

// The envelope follower lives and dies by its filter settings.
// This test flips between low-pass and high-pass modes and ensures
// that DC signals get squashed when they should.

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
