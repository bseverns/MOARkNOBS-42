#include "unity_config.h"
#include <unity.h>
#include <ArduinoJson.h>
#include "sys/report.h"

void setUp() {}
void tearDown() {}

void test_system_report() {
    String json = sys::report();
    StaticJsonDocument<128> doc;
    auto err = deserializeJson(doc, json);
    TEST_ASSERT_FALSE_MESSAGE(err, "Failed to parse report JSON");
    TEST_ASSERT_TRUE(doc.containsKey("fw_version"));
    TEST_ASSERT_TRUE(doc.containsKey("git_sha"));
    TEST_ASSERT_GREATER_THAN(0, strlen(doc["fw_version"] | ""));
    TEST_ASSERT_EQUAL(7, strlen(doc["git_sha"] | ""));
    sys::printReport(Serial);
}

