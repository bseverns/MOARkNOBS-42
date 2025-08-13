#include "SerialStub.h"

#if defined(UNIT_TEST) && !defined(ARDUINO)
[[maybe_unused]] SerialStub Serial;
[[maybe_unused]] SerialStub Serial1;
#endif
