#pragma once

#include <stdint.h>

namespace moar::hal {

using AnalogReadFn = int (*)(uint8_t pin, void *ctx);
using DigitalReadFn = int (*)(uint8_t pin, void *ctx);
using TimeFn = unsigned long (*)(void *ctx);

struct AnalogReadHook {
    AnalogReadFn fn = nullptr;
    void *ctx = nullptr;
};

struct DigitalReadHook {
    DigitalReadFn fn = nullptr;
    void *ctx = nullptr;
};

struct TimeHook {
    TimeFn fn = nullptr;
    void *ctx = nullptr;
};

AnalogReadHook getAnalogReadHook();
void setAnalogReadHook(AnalogReadHook hook);
inline void setAnalogReadHook(AnalogReadFn fn, void *ctx = nullptr) {
    setAnalogReadHook(AnalogReadHook{fn, ctx});
}
void clearAnalogReadHook();
int readAnalog(uint8_t pin);

DigitalReadHook getDigitalReadHook();
void setDigitalReadHook(DigitalReadHook hook);
inline void setDigitalReadHook(DigitalReadFn fn, void *ctx = nullptr) {
    setDigitalReadHook(DigitalReadHook{fn, ctx});
}
void clearDigitalReadHook();
int readDigital(uint8_t pin);

TimeHook getMillisHook();
void setMillisHook(TimeHook hook);
inline void setMillisHook(TimeFn fn, void *ctx = nullptr) {
    setMillisHook(TimeHook{fn, ctx});
}
void clearMillisHook();
unsigned long getMillis();

TimeHook getMicrosHook();
void setMicrosHook(TimeHook hook);
inline void setMicrosHook(TimeFn fn, void *ctx = nullptr) {
    setMicrosHook(TimeHook{fn, ctx});
}
void clearMicrosHook();
unsigned long getMicros();

namespace detail {

template <typename Hook, Hook (*Getter)(), void (*Setter)(Hook)>
class ScopedHook {
  public:
    explicit ScopedHook(Hook hook) : _previous(Getter()), _active(true) { Setter(hook); }
    ScopedHook(const ScopedHook &) = delete;
    ScopedHook &operator=(const ScopedHook &) = delete;
    ScopedHook(ScopedHook &&other) noexcept : _previous(other._previous), _active(other._active) {
        other._active = false;
    }
    ScopedHook &operator=(ScopedHook &&other) noexcept {
        if (this != &other) {
            if (_active) {
                Setter(_previous);
            }
            _previous = other._previous;
            _active = other._active;
            other._active = false;
        }
        return *this;
    }
    ~ScopedHook() {
        if (_active) {
            Setter(_previous);
        }
    }

  private:
    Hook _previous;
    bool _active;
};

} // namespace detail

class ScopedAnalogReadHook : public detail::ScopedHook<AnalogReadHook, getAnalogReadHook, setAnalogReadHook> {
  public:
    explicit ScopedAnalogReadHook(AnalogReadHook hook)
        : detail::ScopedHook<AnalogReadHook, getAnalogReadHook, setAnalogReadHook>(hook) {}
    explicit ScopedAnalogReadHook(AnalogReadFn fn, void *ctx = nullptr)
        : detail::ScopedHook<AnalogReadHook, getAnalogReadHook, setAnalogReadHook>(AnalogReadHook{fn, ctx}) {}
};

class ScopedDigitalReadHook : public detail::ScopedHook<DigitalReadHook, getDigitalReadHook, setDigitalReadHook> {
  public:
    explicit ScopedDigitalReadHook(DigitalReadHook hook)
        : detail::ScopedHook<DigitalReadHook, getDigitalReadHook, setDigitalReadHook>(hook) {}
    explicit ScopedDigitalReadHook(DigitalReadFn fn, void *ctx = nullptr)
        : detail::ScopedHook<DigitalReadHook, getDigitalReadHook, setDigitalReadHook>(DigitalReadHook{fn, ctx}) {}
};

class ScopedMillisHook : public detail::ScopedHook<TimeHook, getMillisHook, setMillisHook> {
  public:
    explicit ScopedMillisHook(TimeHook hook)
        : detail::ScopedHook<TimeHook, getMillisHook, setMillisHook>(hook) {}
    explicit ScopedMillisHook(TimeFn fn, void *ctx = nullptr)
        : detail::ScopedHook<TimeHook, getMillisHook, setMillisHook>(TimeHook{fn, ctx}) {}
};

class ScopedMicrosHook : public detail::ScopedHook<TimeHook, getMicrosHook, setMicrosHook> {
  public:
    explicit ScopedMicrosHook(TimeHook hook)
        : detail::ScopedHook<TimeHook, getMicrosHook, setMicrosHook>(hook) {}
    explicit ScopedMicrosHook(TimeFn fn, void *ctx = nullptr)
        : detail::ScopedHook<TimeHook, getMicrosHook, setMicrosHook>(TimeHook{fn, ctx}) {}
};

} // namespace moar::hal

