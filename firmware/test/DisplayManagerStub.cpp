#if !defined(UNIT_TEST_PROTOCOL_IMPL)
#include "DisplayManager.h"

// Unity tests don't need a full UI stack.
// Stub out the only call MIDIHandler cares about.
void DisplayManager::registerInteraction() {}
#endif
