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
    for (int idx = 0; idx < 194; ++idx) {
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

void test_command_queue_discards_overlong_input_until_newline() {
    testOnly_resetCommandQueue();

    for (size_t idx = 0; idx < SERIAL_BUFFER_SIZE - 1; ++idx) {
        testOnly_ingestSerialByte('A');
    }
    testOnly_ingestSerialByte('B');
    testOnly_ingestSerialByte('\n');

    char buffer[SERIAL_BUFFER_SIZE] = {0};
    TEST_ASSERT_FALSE(dequeueSerialCommand(buffer, sizeof(buffer)));

    const char *nextLine = "HELLO\n";
    for (const char *cursor = nextLine; *cursor != '\0'; ++cursor) {
        testOnly_ingestSerialByte(*cursor);
    }
    expectQueueEntry("HELLO");
}

void test_command_queue_initialize_clears_stale_dmame_state() {
    testOnly_resetCommandQueue();
    testOnly_enqueueSerialCommand("STALE");
    testOnly_corruptCommandQueueState(999, 999, 999);

    initializeCommandQueue();

    char buffer[SERIAL_BUFFER_SIZE] = {0};
    TEST_ASSERT_FALSE(dequeueSerialCommand(buffer, sizeof(buffer)));
    TEST_ASSERT_EQUAL_UINT8(0, serialBufferIndex);
}

void test_command_queue_dequeue_sanitizes_invalid_dmame_state() {
    testOnly_resetCommandQueue();
    testOnly_corruptCommandQueueState(999, 999, 999);

    char buffer[SERIAL_BUFFER_SIZE] = {0};
    TEST_ASSERT_FALSE(dequeueSerialCommand(buffer, sizeof(buffer)));
    TEST_ASSERT_EQUAL_UINT8(0, serialBufferIndex);
}
