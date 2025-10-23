#include "unity_config.h"
#include <unity.h>

#include "Utility.h"
#include <ArduinoJson.h>

void test_bulk_config_assembler_handles_chunks() {
    Utility::BulkConfigAssembler assembler;
    String error;

    TEST_ASSERT_TRUE(assembler.ingestChunk("{\"seq\":1,\"checksum\":\"deadbeef\",\"config\":", error));
    TEST_ASSERT_EQUAL_UINT32(1, assembler.sequenceHint());

    StaticJsonDocument<256> partial;
    auto partialErr = deserializeJson(partial, assembler.payload());
    TEST_ASSERT_TRUE(partialErr == DeserializationError::IncompleteInput);

    TEST_ASSERT_TRUE(assembler.ingestChunk("{\"slots\":[]}}", error));
    StaticJsonDocument<256> doc;
    auto finalErr = deserializeJson(doc, assembler.payload());
    TEST_ASSERT_FALSE(finalErr);
    TEST_ASSERT_EQUAL_STRING("deadbeef", doc["checksum"]);
}

void test_bulk_config_assembler_detects_overflow() {
    Utility::BulkConfigAssembler assembler;
    String chunk;
    chunk.reserve(Utility::kMaxBulkConfigSize + 2);
    chunk = "{";
    for (size_t i = 0; i <= Utility::kMaxBulkConfigSize; ++i) {
        chunk += 'a';
    }
    String error;
    TEST_ASSERT_FALSE(assembler.ingestChunk(chunk, error));
    TEST_ASSERT_TRUE(error == "overflow");
}

void test_format_ack_includes_checksum_and_seq() {
    String ack = Utility::formatAck("cafebabe", 42);
    TEST_ASSERT_NOT_EQUAL(-1, ack.indexOf("\"checksum\":\"cafebabe\""));
    TEST_ASSERT_NOT_EQUAL(-1, ack.indexOf("\"seq\":42"));
}
