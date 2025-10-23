#include "TestHelpers.h"

// The system test sketches borrow these helpers to bootstrap fake hardware
// states without dragging in the full production Globals.cpp.  Keep the wiring
// map here so every test suite agrees on which control pins exist.

// Single source of truth for control button pins used in tests
const uint8_t TEST_CONTROL_PINS[NUM_BUTTONS] = {12, 13, 14, 15, 24, 25};
