// Centralized shim for analog and digital I/O hooks.
// Lets firmware swap in simulated providers during host tests.

#pragma once

#include <stdint.h>

namespace hardware {

using AnalogReadProvider = int (*)(uint8_t pin);
using DigitalReadProvider = int (*)(uint8_t pin);

int readAnalog(uint8_t pin);
int readDigital(uint8_t pin);

AnalogReadProvider setAnalogReadProvider(AnalogReadProvider provider);
DigitalReadProvider setDigitalReadProvider(DigitalReadProvider provider);

AnalogReadProvider currentAnalogReadProvider();
DigitalReadProvider currentDigitalReadProvider();

void resetAnalogReadProvider();
void resetDigitalReadProvider();

class ScopedAnalogReadProvider {
  public:
    explicit ScopedAnalogReadProvider(AnalogReadProvider provider);
    ~ScopedAnalogReadProvider();

    ScopedAnalogReadProvider(const ScopedAnalogReadProvider &) = delete;
    ScopedAnalogReadProvider &operator=(const ScopedAnalogReadProvider &) = delete;

  private:
    AnalogReadProvider previous_;
};

class ScopedDigitalReadProvider {
  public:
    explicit ScopedDigitalReadProvider(DigitalReadProvider provider);
    ~ScopedDigitalReadProvider();

    ScopedDigitalReadProvider(const ScopedDigitalReadProvider &) = delete;
    ScopedDigitalReadProvider &operator=(const ScopedDigitalReadProvider &) = delete;

  private:
    DigitalReadProvider previous_;
};

} // namespace hardware

