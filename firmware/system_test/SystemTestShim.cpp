#include "SystemTestShim.h"

#if defined(FULL_SYSTEM_COMBINED)
#include "Globals.h"

namespace {
SystemTestSummary g_summary{};
bool g_currentFailed = false;
} // namespace

void systemTestBegin() {
    g_summary = {};
    g_currentFailed = false;
    Serial1.begin(SERIAL_BAUD);
    Serial1.println("\n=== System Tests ===");
}

void systemTestRun(const char *name, void (*fn)()) {
    g_currentFailed = false;
    Serial1.print("[TEST] ");
    Serial1.println(name);
    fn();
    g_summary.total++;
    if (g_currentFailed) {
        g_summary.failed++;
        Serial1.println("  FAIL");
    } else {
        Serial1.println("  PASS");
    }
}

SystemTestSummary systemTestEnd() {
    Serial1.printf("=== System Tests: %u total, %u failed ===\n", g_summary.total,
                   g_summary.failed);
    return g_summary;
}

void systemTestFail(const char *expr, const char *file, int line, const char *message) {
    g_currentFailed = true;
    Serial1.print("  ASSERT FAIL: ");
    Serial1.print(expr);
    Serial1.print(" (");
    Serial1.print(file);
    Serial1.print(":");
    Serial1.print(line);
    Serial1.print(") ");
    Serial1.println(message);
}
#endif
