#pragma once
#include <Arduino.h>

namespace sys {
    // Return a JSON string describing firmware version and git revision.
    String report();

    // Print the system report to the given stream (defaults to Serial).
    void printReport(Print &out = Serial);
}

