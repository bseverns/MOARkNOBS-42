#include "unity_config.h"
#include <unity.h>

#include "CommandQueue.h"
#include "Globals.h"
#include <cstring>

namespace {

void expectQueueEntry(const char *expected) {
    char buffer[SERIAL_BUFFER_SIZE] = {0};
    TEST_ASSERT_TRUE(dequeueSerialCommand(buffer, sizeof(buffer)));
    TEST_ASSERT_EQUAL_STRING(expected, buffer);
}

} // namespace

void test_command_queue_drops_oldest_on_overflow() {
    testOnly_resetCommandQueue();

    char line[16];
    for (int idx = 0; idx < 66; ++idx) {
        snprintf(line, sizeof(line), "cmd%02d", idx);
        testOnly_enqueueSerialCommand(line);
    }

    expectQueueEntry("cmd02");
    expectQueueEntry("cmd03");
}

void test_command_queue_ingests_newline_terminated_input() {
    testOnly_resetCommandQueue();

    const char *line = "HELLO\n";
    for (const char *cursor = line; *cursor != '\0'; ++cursor) {
        testOnly_ingestSerialByte(*cursor);
    }

    expectQueueEntry("HELLO");
}

void test_command_queue_ignores_carriage_returns() {
    testOnly_resetCommandQueue();

    const char *line = "SAVE\r\n";
    for (const char *cursor = line; *cursor != '\0'; ++cursor) {
        testOnly_ingestSerialByte(*cursor);
    }

    expectQueueEntry("SAVE");
}

void test_command_queue_flushes_when_buffer_limit_is_hit() {
    testOnly_resetCommandQueue();

    for (size_t idx = 0; idx < SERIAL_BUFFER_SIZE - 1; ++idx) {
        testOnly_ingestSerialByte('A');
    }

    char buffer[SERIAL_BUFFER_SIZE] = {0};
    TEST_ASSERT_TRUE(dequeueSerialCommand(buffer, sizeof(buffer)));
    TEST_ASSERT_EQUAL_UINT(SERIAL_BUFFER_SIZE - 1, strlen(buffer));
    for (size_t idx = 0; idx < strlen(buffer); ++idx) {
        TEST_ASSERT_EQUAL_CHAR('A', buffer[idx]);
    }
}
