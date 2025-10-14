#pragma once

#ifdef UNIT_TEST
#include <cstdint>
#include <cstddef>

struct CRGB {
    uint8_t r;
    uint8_t g;
    uint8_t b;

    constexpr CRGB(uint8_t r_ = 0, uint8_t g_ = 0, uint8_t b_ = 0) : r(r_), g(g_), b(b_) {}

    static const CRGB Black;
    static const CRGB White;
    static const CRGB Red;
    static const CRGB Green;
    static const CRGB Blue;
};

inline const CRGB CRGB::Black = CRGB(0, 0, 0);
inline const CRGB CRGB::White = CRGB(255, 255, 255);
inline const CRGB CRGB::Red = CRGB(255, 0, 0);
inline const CRGB CRGB::Green = CRGB(0, 255, 0);
inline const CRGB CRGB::Blue = CRGB(0, 0, 255);

struct CHSV {
    uint8_t h;
    uint8_t s;
    uint8_t v;

    constexpr CHSV(uint8_t hue, uint8_t sat, uint8_t val) : h(hue), s(sat), v(val) {}

    operator CRGB() const { return CRGB(v, v, v); }
};

struct LEDColorCorrection {};

inline constexpr LEDColorCorrection TypicalLEDStrip = LEDColorCorrection{};

struct WS2812 {};
constexpr int GRB = 0;

class CLEDController {
  public:
    CLEDController &setCorrection(LEDColorCorrection) { return *this; }
};

class FastLEDClass {
  public:
    template <typename CHIPSET, uint8_t DATA_PIN, int COLOR_ORDER>
    CLEDController &addLeds(CRGB *, size_t) {
        return controller;
    }

    void clear() {}
    void show() {}
    void setBrightness(uint8_t) {}

  private:
    CLEDController controller;
};

inline FastLEDClass FastLED;

#else
#include_next <FastLED.h>
#endif
