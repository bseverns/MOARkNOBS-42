#include "TestHelpers.h"

// The system test sketches borrow these helpers to bootstrap fake hardware
// states without dragging in the full production Globals.cpp.  Keep the wiring
// map here so every test suite agrees on which control pins exist.

// WebSerial telemetry defaults to off in the production firmware and is flipped
// on when the browser connects. Our harnesses never compile firmware_main.cpp,
// so we host the flag here to satisfy WebSerial.cpp without pulling in the
// whole runtime.
bool webSerialStreaming = false;

// Single source of truth for control button pins used in tests
const uint8_t TEST_CONTROL_PINS[NUM_BUTTONS] = {12, 13, 14, 15, 24, 25};
