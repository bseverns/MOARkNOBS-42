#include <Globals.h>

// Linker bait: the real Globals.cpp drags in SD/Serial baggage, so the
// unit-test rig fakes the tempo global here.
float g_tappedBPM = 120.0f; // last tapped tempo
