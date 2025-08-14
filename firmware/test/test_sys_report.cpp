#include "unity_config.h"
#include <unity.h>
#include "sys/report.h"
#include "version.h"
#include <string.h>

void setUp() {}
void tearDown() {}

void test_system_report_fields() {
    const char* json = systemReportJSON();
    TEST_ASSERT_NOT_NULL(json);
    TEST_ASSERT_TRUE(strstr(json, "\"fw\""));
    TEST_ASSERT_TRUE(strstr(json, "\"git\""));
    printf("%s\n", json);
}

