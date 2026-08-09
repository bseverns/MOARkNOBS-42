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
    static const CRGB Yellow;
};

inline const CRGB CRGB::Black = CRGB(0, 0, 0);
inline const CRGB CRGB::White = CRGB(255, 255, 255);
inline const CRGB CRGB::Red = CRGB(255, 0, 0);
inline const CRGB CRGB::Green = CRGB(0, 255, 0);
inline const CRGB CRGB::Blue = CRGB(0, 0, 255);
inline const CRGB CRGB::Yellow = CRGB(255, 255, 0);

inline CRGB blend(const CRGB &a, const CRGB &b, uint8_t amountOfB) {
    const uint16_t weightB = amountOfB;
    const uint16_t weightA = 255 - weightB;
    auto blendChannel = [&](uint8_t channelA, uint8_t channelB) {
        return static_cast<uint8_t>((channelA * weightA + channelB * weightB) / 255);
    };

    return CRGB(blendChannel(a.r, b.r), blendChannel(a.g, b.g), blendChannel(a.b, b.b));
}

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
struct OCTOWS2811 {};
constexpr int GRB = 0;

inline uint8_t sin8(uint8_t phase) {
    const uint8_t quadrant = phase >> 6U;
    const uint8_t offset = phase & 0x3FU;
    switch (quadrant) {
    case 0: return static_cast<uint8_t>(128U + offset * 2U);
    case 1: return static_cast<uint8_t>(255U - offset * 2U);
    case 2: return static_cast<uint8_t>(127U - offset * 2U);
    default: return static_cast<uint8_t>(offset * 2U);
    }
}

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

    template <typename CHIPSET> CLEDController &addLeds(CRGB *, size_t) { return controller; }

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
