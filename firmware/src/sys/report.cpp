#include "sys/report.h"
#include <ArduinoJson.h>
#include "version.h"

namespace sys {
String report() {
    StaticJsonDocument<128> doc;
    doc["fw_version"] = FW_VERSION;
    doc["git_sha"] = GIT_SHA;
    String out;
    serializeJson(doc, out);
    return out;
}

void printReport(Print &out) {
    out.println(report());
}
}

