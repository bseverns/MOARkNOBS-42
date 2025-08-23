#include "SerialStub.h"

#if defined(UNIT_TEST) && !defined(ARDUINO)
SerialStub Serial;
SerialStub Serial1;
#endif // UNIT_TEST && !ARDUINO
