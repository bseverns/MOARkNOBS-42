#include "Hardware/IO.h"

#if defined(ARDUINO)
#include <Arduino.h>
#endif

namespace {

int passthroughAnalog(uint8_t pin) {
#if defined(ARDUINO)
    return ::analogRead(pin);
#else
    (void)pin;
    return 0;
#endif
}

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

int readAnalog(uint8_t pin) { return gAnalogProvider ? gAnalogProvider(pin) : 0; }

int readDigital(uint8_t pin) { return gDigitalProvider ? gDigitalProvider(pin) : 0; }

AnalogReadProvider setAnalogReadProvider(AnalogReadProvider provider) {
    AnalogReadProvider previous = gAnalogProvider;
    gAnalogProvider = provider ? provider : passthroughAnalog;
    return previous;
}

DigitalReadProvider setDigitalReadProvider(DigitalReadProvider provider) {
    DigitalReadProvider previous = gDigitalProvider;
    gDigitalProvider = provider ? provider : passthroughDigital;
    return previous;
}

AnalogReadProvider currentAnalogReadProvider() { return gAnalogProvider; }

DigitalReadProvider currentDigitalReadProvider() { return gDigitalProvider; }

void resetAnalogReadProvider() { gAnalogProvider = passthroughAnalog; }

void resetDigitalReadProvider() { gDigitalProvider = passthroughDigital; }

ScopedAnalogReadProvider::ScopedAnalogReadProvider(AnalogReadProvider provider)
    : previous_(setAnalogReadProvider(provider)) {}

ScopedAnalogReadProvider::~ScopedAnalogReadProvider() { setAnalogReadProvider(previous_); }

ScopedDigitalReadProvider::ScopedDigitalReadProvider(DigitalReadProvider provider)
    : previous_(setDigitalReadProvider(provider)) {}

ScopedDigitalReadProvider::~ScopedDigitalReadProvider() { setDigitalReadProvider(previous_); }

} // namespace hardware
