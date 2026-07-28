#if !defined(NATIVE_BIQUAD_TEST)

#include "SerialStub.h"

#if defined(UNIT_TEST) && !defined(ARDUINO)
SerialStub Serial;
SerialStub Serial1;

#endif // UNIT_TEST && !ARDUINO
#endif // !NATIVE_BIQUAD_TEST
