// Hardware/IO.cpp abstracts raw Arduino calls so the rest of the firmware can
// swap in fakes during tests. The comments keep the why front-and-center:
// function pointers as dependency injection, scoped helpers to automatically
// restore the previous provider, and no hidden static state beyond what we
// expose on purpose.

#include "Hardware/IO.h"

#if defined(ARDUINO)
#include <Arduino.h>
#endif

namespace {

// Default analog provider that simply forwards to Arduino until tests swap in a
// fake implementation.
int passthroughAnalog(uint8_t pin) {
#if defined(ARDUINO)
    return ::analogRead(pin);
#else
    (void)pin;
    return 0;
#endif
}

// Default digital provider paired with `passthroughAnalog` for the same reason.
int passthroughDigital(uint8_t pin) {
#if defined(ARDUINO)
    return ::digitalRead(pin);
#else
    (void)pin;
    return 0;
#endif
}

hardware::AnalogReadProvider gAnalogProvider = passthroughAnalog;
hardware::DigitalReadProvider gDigitalProvider = passthroughDigital;

} // namespace

namespace hardware {

// Route analog reads through the currently installed provider so production and
// test code call the same API.
int readAnalog(uint8_t pin) { return gAnalogProvider ? gAnalogProvider(pin) : 0; }

// Route digital reads through the currently installed provider.
int readDigital(uint8_t pin) { return gDigitalProvider ? gDigitalProvider(pin) : 0; }

// Swap the active analog provider and hand the old one back to the caller so
// scoped helpers can restore it later.
AnalogReadProvider setAnalogReadProvider(AnalogReadProvider provider) {
    AnalogReadProvider previous = gAnalogProvider;
    gAnalogProvider = provider ? provider : passthroughAnalog;
    return previous;
}

// Digital equivalent of `setAnalogReadProvider`.
DigitalReadProvider setDigitalReadProvider(DigitalReadProvider provider) {
    DigitalReadProvider previous = gDigitalProvider;
    gDigitalProvider = provider ? provider : passthroughDigital;
    return previous;
}

// Expose the current analog provider for tests that need to snapshot/chain it.
AnalogReadProvider currentAnalogReadProvider() { return gAnalogProvider; }

// Expose the current digital provider for the same reason.
DigitalReadProvider currentDigitalReadProvider() { return gDigitalProvider; }

// Restore production analog behavior after a test override.
void resetAnalogReadProvider() { gAnalogProvider = passthroughAnalog; }

// Restore production digital behavior after a test override.
void resetDigitalReadProvider() { gDigitalProvider = passthroughDigital; }

// Install a temporary analog provider for the lifetime of this scope object.
ScopedAnalogReadProvider::ScopedAnalogReadProvider(AnalogReadProvider provider)
    : previous_(setAnalogReadProvider(provider)) {}

// Put the previous analog provider back when the guard goes out of scope.
ScopedAnalogReadProvider::~ScopedAnalogReadProvider() { setAnalogReadProvider(previous_); }

// Install a temporary digital provider for the lifetime of this scope object.
ScopedDigitalReadProvider::ScopedDigitalReadProvider(DigitalReadProvider provider)
    : previous_(setDigitalReadProvider(provider)) {}

// Put the previous digital provider back when the guard goes out of scope.
ScopedDigitalReadProvider::~ScopedDigitalReadProvider() { setDigitalReadProvider(previous_); }

} // namespace hardware
