#include "SerialStub.h"

#if defined(UNIT_TEST) && !defined(ARDUINO)
SerialStub Serial;
SerialStub Serial1;
#elif defined(UNIT_TEST)
SerialStub &Serial = Serial1;
#endif
