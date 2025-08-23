#pragma once
#include <Arduino.h>

namespace sys {
// Return a JSON string packed with firmware, build, and hardware info.
String report();

// Print the system report to the given stream (defaults to Serial).
void printReport(Print &out = Serial);
} // namespace sys
