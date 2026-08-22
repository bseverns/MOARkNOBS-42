#include "unity_config.h"
#include <unity.h>

#include "Log.h"
#include "protocol/ConfigBulkTransport.h"

namespace {
void assertLogContains(const char *snippet) {
    TEST_ASSERT_NOT_EQUAL_MESSAGE(-1, peekTestLogBuffer().indexOf(snippet), snippet);
}

void beginTest() {
    testOnlyResetConfigBulkTransport();
    clearTestLogBuffer();
}
} // namespace

void test_config_bulk_parse_failure_discards_partial_transaction() {
    beginTest();
    handleSetAllBulkCommand("SET_ALL {\"seq\":101,\"config_id\":\"parse-case\",");
    TEST_ASSERT_TRUE(testOnlyConfigBulkTransportInProgress());

    handleSetAllBulkCommand("SET_ALL bad}");
    assertLogContains("\"code\":\"parse\"");
    TEST_ASSERT_FALSE(testOnlyConfigBulkTransportInProgress());

    clearTestLogBuffer();
    handleSetAllBulkCommand("SET_ALL \"config\":{}}");
    assertLogContains("\"code\":\"orphan\"");
    TEST_ASSERT_FALSE(testOnlyConfigBulkTransportInProgress());
}

void test_config_bulk_missing_identity_discards_complete_transaction() {
    beginTest();
    handleSetAllBulkCommand("SET_ALL {\"seq\":102,\"config\":{}}");
    assertLogContains("\"code\":\"checksum\"");
    TEST_ASSERT_FALSE(testOnlyConfigBulkTransportInProgress());
}

void test_config_bulk_abort_is_idempotent() {
    beginTest();
    handleSetAllBulkCommand("SET_ALL {\"seq\":103,");
    TEST_ASSERT_TRUE(testOnlyConfigBulkTransportInProgress());

    handleAbortSetAllBulkCommand("ABORT_SET_ALL");
    assertLogContains("\"aborted\":true");
    assertLogContains("\"seq\":103");
    TEST_ASSERT_FALSE(testOnlyConfigBulkTransportInProgress());

    clearTestLogBuffer();
    handleAbortSetAllBulkCommand("ABORT_SET_ALL");
    assertLogContains("\"aborted\":false");
    assertLogContains("\"seq\":0");
    TEST_ASSERT_FALSE(testOnlyConfigBulkTransportInProgress());
}

void test_config_bulk_timeout_resets_upload_and_reports_sequence() {
    beginTest();
    handleSetAllBulkCommand("SET_ALL {\"seq\":104,");
    TEST_ASSERT_TRUE(testOnlyConfigBulkTransportInProgress());

    clearTestLogBuffer();
    testOnlyForceConfigBulkTimeout();
    assertLogContains("\"code\":\"timeout\"");
    assertLogContains("\"seq\":104");
    TEST_ASSERT_FALSE(testOnlyConfigBulkTransportInProgress());
}

void test_config_bulk_duplicate_retry_replays_stable_ack_without_apply() {
    beginTest();
    testOnlySeedConfigBulkAck(105, "stable-id", "applied-state", 37);
    const String retry =
        "SET_ALL {\"seq\":105,\"config_id\":\"stable-id\",\"config\":{}}";

    handleSetAllBulkCommand(retry);
    const String firstAck = peekTestLogBuffer();
    assertLogContains("\"status\":\"ok\"");
    assertLogContains("\"checksum\":\"stable-id\"");
    assertLogContains("\"applied_checksum\":\"applied-state\"");
    assertLogContains("\"storage_generation\":37");
    TEST_ASSERT_FALSE(testOnlyConfigBulkTransportInProgress());

    clearTestLogBuffer();
    handleSetAllBulkCommand(retry);
    TEST_ASSERT_EQUAL_STRING(firstAck.c_str(), peekTestLogBuffer().c_str());
    TEST_ASSERT_FALSE(testOnlyConfigBulkTransportInProgress());
}
