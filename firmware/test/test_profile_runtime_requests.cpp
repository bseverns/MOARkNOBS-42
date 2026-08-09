#include "unity_config.h"
#include <unity.h>

#include "ProfileRuntimeRequests.h"

void test_profile_runtime_requests_preserve_independent_pending_work() {
    ProfileRuntimeRequests requests;
    requests.requestReload();
    requests.requestSave();

    TEST_ASSERT_TRUE(requests.takeReload());
    TEST_ASSERT_FALSE(requests.reloadPending());
    TEST_ASSERT_TRUE(requests.savePending());
    TEST_ASSERT_TRUE(requests.takeSave());
}

void test_profile_runtime_requests_take_consumes_once() {
    ProfileRuntimeRequests requests;
    requests.requestSave();

    TEST_ASSERT_TRUE(requests.takeSave());
    TEST_ASSERT_FALSE(requests.takeSave());
    TEST_ASSERT_FALSE(requests.savePending());
}
