/*
sys::report() spits out a JSON blob stuffed with firmware and build trivia
for anyone poking around in diagnostics. Public API lives in
firmware/include/sys/report.h, go party there.
*/
#include "sys/report.h"
#include <ArduinoJson.h>
#include "version.h"

namespace sys {
// Pack the current firmware/build identity into a tiny JSON blob for serial
// diagnostics, browser tooling, and support captures.
String report() {
    StaticJsonDocument<256> doc;
    doc["fw_version"] = FW_VERSION_STR; // firmware release tag
    doc["git_sha"] = GIT_SHA_STR;       // source control commit hash

#if defined(ARDUINO_TEENSY40)
    doc["board"] = "teensy40"; // target board identifier
#elif defined(ARDUINO_TEENSY41)
    doc["board"] = "teensy41"; // target board identifier
#else
    doc["board"] = "unknown"; // fallback if board is undetected
#endif

#ifdef __IMXRT1062__
    doc["mcu"] = "IMXRT1062"; // MCU family
#endif

    doc["f_cpu_hz"] = static_cast<uint32_t>(F_CPU); // CPU frequency in Hz
    doc["build_time"] = __DATE__ " " __TIME__;      // when this binary was built
    doc["compiler"] = __VERSION__;                  // compiler version string

#ifdef ARDUINO
    doc["arduino"] = ARDUINO; // Arduino core version
#endif

#ifdef PIOENV
    doc["pio_env"] = PIOENV; // PlatformIO environment name
#endif

    String out;
    out.reserve(128);
    if (serializeJson(doc, out) == 0) {
        out = "{}";
    }
    return out;
}

// Convenience wrapper for streams that want the report as a full line.
void printReport(Print &out) { out.println(report()); }
} // namespace sys
