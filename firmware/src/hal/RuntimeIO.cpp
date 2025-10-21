#include "hal/RuntimeIO.h"

#include <Arduino.h>

namespace moar::hal {
namespace {

AnalogReadHook gAnalogHook = {};
DigitalReadHook gDigitalHook = {};
TimeHook gMillisHook = {};
TimeHook gMicrosHook = {};

int defaultAnalogRead(uint8_t pin, void *) { return ::analogRead(pin); }
int defaultDigitalRead(uint8_t pin, void *) { return ::digitalRead(pin); }
unsigned long defaultMillis(void *) { return ::millis(); }
unsigned long defaultMicros(void *) { return ::micros(); }

} // namespace

AnalogReadHook getAnalogReadHook() { return gAnalogHook; }

void setAnalogReadHook(AnalogReadHook hook) { gAnalogHook = hook; }

void clearAnalogReadHook() { gAnalogHook = {}; }

int readAnalog(uint8_t pin) {
    if (gAnalogHook.fn) {
        return gAnalogHook.fn(pin, gAnalogHook.ctx);
    }
    return defaultAnalogRead(pin, nullptr);
}

DigitalReadHook getDigitalReadHook() { return gDigitalHook; }

void setDigitalReadHook(DigitalReadHook hook) { gDigitalHook = hook; }

void clearDigitalReadHook() { gDigitalHook = {}; }

int readDigital(uint8_t pin) {
    if (gDigitalHook.fn) {
        return gDigitalHook.fn(pin, gDigitalHook.ctx);
    }
    return defaultDigitalRead(pin, nullptr);
}

TimeHook getMillisHook() { return gMillisHook; }

void setMillisHook(TimeHook hook) { gMillisHook = hook; }

void clearMillisHook() { gMillisHook = {}; }

unsigned long getMillis() {
    if (gMillisHook.fn) {
        return gMillisHook.fn(gMillisHook.ctx);
    }
    return defaultMillis(nullptr);
}

TimeHook getMicrosHook() { return gMicrosHook; }

void setMicrosHook(TimeHook hook) { gMicrosHook = hook; }

void clearMicrosHook() { gMicrosHook = {}; }

unsigned long getMicros() {
    if (gMicrosHook.fn) {
        return gMicrosHook.fn(gMicrosHook.ctx);
    }
    return defaultMicros(nullptr);
}

} // namespace moar::hal

