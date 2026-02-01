#pragma once

#if defined(FULL_SYSTEM_COMBINED)
#include <Arduino.h>
#include <cmath>
#include <cstdint>
#include <cstdio>

struct SystemTestSummary {
    uint16_t total = 0;
    uint16_t failed = 0;
};

void systemTestBegin();
void systemTestRun(const char *name, void (*fn)());
SystemTestSummary systemTestEnd();
void systemTestFail(const char *expr, const char *file, int line, const char *message);

inline void systemTestAssertTrue(bool cond, const char *expr, const char *file, int line) {
    if (!cond) {
        systemTestFail(expr, file, line, "expected true");
    }
}

inline void systemTestAssertFalse(bool cond, const char *expr, const char *file, int line) {
    if (cond) {
        systemTestFail(expr, file, line, "expected false");
    }
}

inline void systemTestAssertEqualLong(long expected, long actual, const char *expr,
                                          const char *file, int line) {
    if (expected != actual) {
        char msg[96];
        snprintf(msg, sizeof(msg), "expected %ld got %ld", expected, actual);
        systemTestFail(expr, file, line, msg);
    }
}

inline void systemTestAssertEqualUnsigned(unsigned long expected, unsigned long actual,
                                          const char *expr, const char *file, int line) {
    if (expected != actual) {
        char msg[96];
        snprintf(msg, sizeof(msg), "expected %lu got %lu", expected, actual);
        systemTestFail(expr, file, line, msg);
    }
}

inline void systemTestAssertFloatWithin(float delta, float expected, float actual, const char *expr,
                                        const char *file, int line) {
    if (fabsf(expected - actual) > delta) {
        char msg[96];
        snprintf(msg, sizeof(msg), "expected %.4f got %.4f (±%.4f)", expected, actual, delta);
        systemTestFail(expr, file, line, msg);
    }
}

inline void systemTestAssertIntWithin(int delta, int expected, int actual, const char *expr,
                                      const char *file, int line) {
    if (abs(expected - actual) > delta) {
        char msg[96];
        snprintf(msg, sizeof(msg), "expected %d got %d (±%d)", expected, actual, delta);
        systemTestFail(expr, file, line, msg);
    }
}

inline void systemTestAssertLessThan(long threshold, long actual, const char *expr,
                                     const char *file, int line) {
    if (!(actual < threshold)) {
        char msg[96];
        snprintf(msg, sizeof(msg), "expected < %ld got %ld", threshold, actual);
        systemTestFail(expr, file, line, msg);
    }
}

#define UNITY_BEGIN() systemTestBegin()
#define UNITY_END() systemTestEnd()
#define RUN_TEST(fn) systemTestRun(#fn, fn)

#define TEST_ASSERT_TRUE(cond) systemTestAssertTrue((cond), #cond, __FILE__, __LINE__)
#define TEST_ASSERT_FALSE(cond) systemTestAssertFalse((cond), #cond, __FILE__, __LINE__)
#define TEST_ASSERT_EQUAL(expected, actual)                                                     \
    systemTestAssertEqualLong((long)(expected), (long)(actual), #expected " == " #actual,        \
                              __FILE__, __LINE__)
#define TEST_ASSERT_EQUAL_INT(expected, actual)                                                  \
    systemTestAssertEqualLong((long)(expected), (long)(actual), #expected " == " #actual,        \
                              __FILE__, __LINE__)
#define TEST_ASSERT_EQUAL_UINT8(expected, actual)                                                \
    systemTestAssertEqualUnsigned((unsigned long)(expected), (unsigned long)(actual),            \
                                  #expected " == " #actual, __FILE__, __LINE__)
#define TEST_ASSERT_EQUAL_UINT(expected, actual)                                                 \
    systemTestAssertEqualUnsigned((unsigned long)(expected), (unsigned long)(actual),            \
                                  #expected " == " #actual, __FILE__, __LINE__)
#define TEST_ASSERT_EQUAL_UINT16(expected, actual)                                               \
    systemTestAssertEqualUnsigned((unsigned long)(expected), (unsigned long)(actual),            \
                                  #expected " == " #actual, __FILE__, __LINE__)
#define TEST_ASSERT_EQUAL_UINT32(expected, actual)                                               \
    systemTestAssertEqualUnsigned((unsigned long)(expected), (unsigned long)(actual),            \
                                  #expected " == " #actual, __FILE__, __LINE__)
#define TEST_ASSERT_FLOAT_WITHIN(delta, expected, actual)                                        \
    systemTestAssertFloatWithin((float)(delta), (float)(expected), (float)(actual),              \
                                #expected " ~= " #actual, __FILE__, __LINE__)
#define TEST_ASSERT_INT_WITHIN(delta, expected, actual)                                          \
    systemTestAssertIntWithin((int)(delta), (int)(expected), (int)(actual),                       \
                              #expected " ~= " #actual, __FILE__, __LINE__)
#define TEST_ASSERT_LESS_THAN(threshold, actual)                                                 \
    systemTestAssertLessThan((long)(threshold), (long)(actual),                                   \
                             #actual " < " #threshold, __FILE__, __LINE__)

#else
#include <unity.h>
#endif
