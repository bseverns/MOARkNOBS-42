#include <unity.h>

#include "ModulationTransportPolicy.h"

#include <array>
#include <cstdint>
#include <limits>

namespace {
using Policy = ModulationTransportPolicy<42>;
}

void test_transport_reset_uses_current_clock_and_initial_budget() {
    Policy policy;
    policy.reset(5000);

    TEST_ASSERT_EQUAL_UINT16(Policy::kInitialBytes, policy.availableBytes());
    TEST_ASSERT_EQUAL_UINT32(5000, policy.updatedAtMs());
    policy.refill(5000);
    TEST_ASSERT_EQUAL_UINT16(Policy::kInitialBytes, policy.availableBytes());
}

void test_transport_refill_caps_and_handles_clock_wrap() {
    Policy policy;
    policy.reset(100);
    policy.refill(105);
    TEST_ASSERT_EQUAL_UINT16(30, policy.availableBytes());
    TEST_ASSERT_TRUE(policy.recordSuccess(12));
    TEST_ASSERT_EQUAL_UINT16(18, policy.availableBytes());
    policy.refill(1000);
    TEST_ASSERT_EQUAL_UINT16(Policy::kCapacityBytes, policy.availableBytes());

    policy.reset(std::numeric_limits<uint32_t>::max() - 2U);
    policy.refill(2);
    TEST_ASSERT_EQUAL_UINT16(30, policy.availableBytes());
}

void test_transport_prioritizes_notes_and_rotates_fairly() {
    Policy policy;
    policy.reset(0);
    std::array<Policy::Candidate, 42> candidates{};
    candidates[0] = {true, false, 12, 3};
    candidates[4] = {true, true, 90, 6};
    candidates[5] = {true, false, 64, 3};

    TEST_ASSERT_EQUAL_INT(4, policy.nextCandidate(candidates, true));
    policy.recordAttempt(4);
    candidates[4].pending = false;
    TEST_ASSERT_EQUAL_INT(5, policy.nextCandidate(candidates, false));
    policy.recordAttempt(5);
    candidates[5].pending = false;
    TEST_ASSERT_EQUAL_INT(0, policy.nextCandidate(candidates, false));
}

void test_transport_failed_attempt_preserves_tokens() {
    Policy policy;
    policy.reset(200);
    const uint16_t before = policy.availableBytes();

    policy.recordAttempt(41);
    TEST_ASSERT_EQUAL_UINT16(before, policy.availableBytes());
    TEST_ASSERT_EQUAL_UINT8(0, policy.cursor());
    TEST_ASSERT_FALSE(policy.recordSuccess(static_cast<uint8_t>(before + 1U)));
    TEST_ASSERT_EQUAL_UINT16(before, policy.availableBytes());
}
