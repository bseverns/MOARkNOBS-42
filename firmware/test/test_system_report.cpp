#include "unity_config.h"
#include <unity.h>
#include <ArduinoJson.h>
#include "sys/report.h"

// sys::report() is the firmware's "tell me who you are" API.  Make sure the
// JSON blob stays parseable and keeps the identity fields we promise tooling.
void test_system_report() {
    String json = sys::report();
    StaticJsonDocument<256> doc;
    auto err = deserializeJson(doc, json);
    TEST_ASSERT_FALSE_MESSAGE(err, "Failed to parse report JSON");
    TEST_ASSERT_TRUE(doc.containsKey("fw_version"));
    TEST_ASSERT_TRUE(doc.containsKey("git_sha"));
    TEST_ASSERT_TRUE(doc.containsKey("board"));
    TEST_ASSERT_TRUE(doc.containsKey("f_cpu_hz"));
    TEST_ASSERT_TRUE(doc.containsKey("build_time"));
    TEST_ASSERT_GREATER_THAN(0, strlen(doc["fw_version"] | ""));
    TEST_ASSERT_EQUAL(7, strlen(doc["git_sha"] | ""));
    TEST_ASSERT_GREATER_THAN(0, strlen(doc["board"] | ""));
    TEST_ASSERT_GREATER_THAN(0, doc["f_cpu_hz"].as<long>());
    TEST_ASSERT_GREATER_THAN(0, strlen(doc["build_time"] | ""));
    // Blast the report over Serial1 so the Teensy never blocks on the USB CDC
    // port during tests. The Unity shim already pipes Serial1 through
    // unityOutputChar, so we reuse that lane instead of poking plain Serial.
    sys::printReport(Serial1);
}
