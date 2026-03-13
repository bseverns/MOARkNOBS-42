#include "Log.h"

#include <cstdarg>
#include <cstdio>

namespace {
String g_testLogBuffer;
}

void clearTestLogBuffer() { g_testLogBuffer = ""; }

const String &peekTestLogBuffer() { return g_testLogBuffer; }

void appendTestLogBuffer(const char *text) {
    if (text) {
        g_testLogBuffer += text;
    }
}

void appendTestLogBuffer(const String &text) { g_testLogBuffer += text; }

void testLogPrintf(const char *format, ...) {
    if (!format) {
        return;
    }

    char buffer[512];
    va_list args;
    va_start(args, format);
    const int written = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    if (written > 0) {
        g_testLogBuffer += buffer;
    }
}
