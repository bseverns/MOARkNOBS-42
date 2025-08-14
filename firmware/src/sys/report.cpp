#include "sys/report.h"
#include <ArduinoJson.h>
#include "version.h"

namespace sys {
String report() {
    StaticJsonDocument<256> doc;
    doc["fw_version"] = FW_VERSION_STR;
    doc["git_sha"] = GIT_SHA_STR;

#if defined(ARDUINO_TEENSY40)
    doc["board"] = "teensy40";
#elif defined(ARDUINO_TEENSY41)
    doc["board"] = "teensy41";
#else
    doc["board"] = "unknown";
#endif

#ifdef __IMXRT1062__
    doc["mcu"] = "IMXRT1062";
#endif

    doc["f_cpu_hz"] = static_cast<uint32_t>(F_CPU);
    doc["build_time"] = __DATE__ " " __TIME__;
    doc["compiler"] = __VERSION__;

#ifdef ARDUINO
    doc["arduino"] = ARDUINO;
#endif

#ifdef PIOENV
    doc["pio_env"] = PIOENV;
#endif

    String out;
    serializeJson(doc, out);
    return out;
}

void printReport(Print &out) {
    out.println(report());
}
}

