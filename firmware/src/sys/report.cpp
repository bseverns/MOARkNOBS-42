#include "sys/report.h"
#include "version.h"
#include <stdio.h>

const char* systemReportJSON() {
    static char buf[96];
    snprintf(buf, sizeof(buf), "{\"fw\":\"%s\",\"git\":\"%s\"}", FW_VERSION, GIT_SHA);
    return buf;
}

