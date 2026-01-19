#include "unity_config.h"
#include <unity.h>

#define private public
#include "EnvelopeFollower.h"
#undef private

#include "Hardware/IO.h"

void test_envelope_apply_to_cc_clamps_upper_bound() {
    EnvelopeFollower ef(A0, nullptr, 0);
    ef.toggleActive(true);
    ef.currentEnvelopeLevel = 70;
    uint8_t ccValue = 90;
    ef.applyToCC(0, ccValue);
    TEST_ASSERT_EQUAL_UINT8(127, ccValue);
}

void test_envelope_apply_to_cc_clamps_lower_bound() {
    EnvelopeFollower ef(A0, nullptr, 0);
    ef.toggleActive(true);
    ef.currentEnvelopeLevel = -80;
    uint8_t ccValue = 30;
    ef.applyToCC(0, ccValue);
    TEST_ASSERT_EQUAL_UINT8(0, ccValue);
}

void test_envelope_apply_to_cc_is_noop_when_inactive() {
    EnvelopeFollower ef(A0, nullptr, 0);
    ef.toggleActive(false);
    ef.currentEnvelopeLevel = 127;
    uint8_t ccValue = 64;
    ef.applyToCC(0, ccValue);
    TEST_ASSERT_EQUAL_UINT8(64, ccValue);
}
