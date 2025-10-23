#include "OctoWS2811.h"

#include <algorithm>
#include <cstring>

namespace {
constexpr uint8_t kColorOrderTable[30][4] = {
    {0, 1, 2, 3}, {0, 2, 1, 3}, {1, 0, 2, 3}, {1, 2, 0, 3}, {2, 0, 1, 3}, {2, 1, 0, 3},
    {0, 1, 2, 3}, {0, 2, 1, 3}, {1, 0, 2, 3}, {1, 2, 0, 3}, {2, 0, 1, 3}, {2, 1, 0, 3},
    {3, 0, 1, 2}, {3, 0, 2, 1}, {3, 1, 0, 2}, {3, 1, 2, 0}, {3, 2, 0, 1}, {3, 2, 1, 0},
    {0, 3, 1, 2}, {0, 3, 2, 1}, {1, 3, 0, 2}, {1, 3, 2, 0}, {2, 3, 0, 1}, {2, 3, 1, 0},
    {0, 1, 3, 2}, {0, 2, 3, 1}, {1, 0, 3, 2}, {1, 2, 3, 0}, {2, 0, 3, 1}, {2, 1, 3, 0},
};
}

const uint8_t OctoWS2811::defaultPinList[8] = {2, 14, 7, 8, 6, 20, 21, 5};

OctoWS2811::OctoWS2811(uint32_t numPerStrip, void *frameBuffer, void *drawBuffer, uint8_t config)
    : OctoWS2811(numPerStrip, frameBuffer, drawBuffer, config, defaultPinList, 8) {}

OctoWS2811::OctoWS2811(uint32_t numPerStrip, void *frameBufferIn, void *drawBufferIn,
                       uint8_t config, const uint8_t *pinList, uint8_t pinCount)
    : driver(nullptr), ledsPerStrip(numPerStrip), totalPixels(numPerStrip * pinCount),
      params(config), hasWhite((config & 0x3F) >= WS2811_RGBW), pinsActive(pinCount),
      brightness(255), colorBalance(0xFFFFFF), frameBuffer(static_cast<uint8_t *>(frameBufferIn)),
      drawBuffer(static_cast<uint8_t *>(drawBufferIn)) {
    (void)frameBuffer; // FastLED passes a frame buffer, but ObjectFLED only needs the draw buffer.
    if (pinsActive == 0 || pinsActive > kMaxPins) {
        pinsActive = 8;
    }
    std::fill(std::begin(pins), std::end(pins), 0);
    if (pinList) {
        std::memcpy(pins, pinList, pinsActive);
    } else {
        std::memcpy(pins, defaultPinList, pinsActive);
    }
    driver = new OctoWS2811Driver(static_cast<uint16_t>(ledsPerStrip), drawBuffer, params,
                                  pinsActive, pins);
}

OctoWS2811::~OctoWS2811() { delete driver; }

void OctoWS2811::begin() {
    if (driver) {
        driver->begin();
        applyConfig();
    }
}

void OctoWS2811::begin(uint16_t latchMicros) {
    if (driver) {
        driver->begin(latchMicros);
        applyConfig();
    }
}

void OctoWS2811::begin(double overclock, uint16_t latchMicros) {
    if (driver) {
        driver->begin(overclock, latchMicros);
        applyConfig();
    }
}

void OctoWS2811::begin(uint16_t periodNS, uint16_t t0hNS, uint16_t t1hNS, uint16_t latchMicros) {
    if (driver) {
        driver->begin(periodNS, t0hNS, t1hNS, latchMicros);
        applyConfig();
    }
}

void OctoWS2811::show() {
    if (driver) {
        driver->show();
    }
}

int OctoWS2811::busy() { return driver ? driver->busy() : 0; }

void OctoWS2811::setPixel(uint32_t num, uint8_t red, uint8_t green, uint8_t blue) {
    setPixelInternal(num, red, green, blue, 0);
}

void OctoWS2811::setPixel(uint32_t num, uint8_t red, uint8_t green, uint8_t blue, uint8_t white) {
    setPixelInternal(num, red, green, blue, white);
}

void OctoWS2811::setPixel(uint32_t num, uint32_t color) {
    uint8_t white = hasWhite ? static_cast<uint8_t>((color >> 24) & 0xFF) : 0;
    uint8_t red = static_cast<uint8_t>((color >> 16) & 0xFF);
    uint8_t green = static_cast<uint8_t>((color >> 8) & 0xFF);
    uint8_t blue = static_cast<uint8_t>(color & 0xFF);
    setPixelInternal(num, red, green, blue, white);
}

uint32_t OctoWS2811::getPixel(uint32_t num) const {
    if (!drawBuffer || num >= totalPixels) {
        return 0;
    }
    const uint8_t order = params & 0x3F;
    const uint8_t bytesPerPixel = hasWhite ? 4 : 3;
    const uint8_t *src = drawBuffer + num * bytesPerPixel;
    uint8_t components[4] = {0, 0, 0, 0};

    uint8_t mappedOrder = order < 30 ? order : 0;
    for (uint8_t i = 0; i < bytesPerPixel; ++i) {
        components[kColorOrderTable[mappedOrder][i]] = src[i];
    }

    uint32_t packed = (static_cast<uint32_t>(components[0]) << 16) |
                      (static_cast<uint32_t>(components[1]) << 8) |
                      static_cast<uint32_t>(components[2]);
    if (hasWhite) {
        packed |= static_cast<uint32_t>(components[3]) << 24;
    }
    return packed;
}

uint8_t *OctoWS2811::getPixels() { return drawBuffer; }

void OctoWS2811::setBrightness(uint8_t value) {
    brightness = value;
    if (driver) {
        driver->setBrightness(value);
    }
}

uint8_t OctoWS2811::getBrightness() const { return driver ? driver->getBrightness() : brightness; }

void OctoWS2811::setColorBalance(uint32_t value) {
    colorBalance = value;
    if (driver) {
        driver->setColorBalance(value);
    }
}

uint32_t OctoWS2811::getColorBalance() const {
    return driver ? driver->getColorBalance() : colorBalance;
}

void OctoWS2811::setPins(const uint8_t *pinList, uint8_t count) {
    if (count == 0 || count > kMaxPins) {
        count = 8;
    }
    pinsActive = count;
    std::fill(std::begin(pins), std::end(pins), 0);
    if (pinList) {
        std::memcpy(pins, pinList, pinsActive);
    } else {
        std::memcpy(pins, defaultPinList, pinsActive);
    }
    totalPixels = ledsPerStrip * pinsActive;
    delete driver;
    driver = new OctoWS2811Driver(static_cast<uint16_t>(ledsPerStrip), drawBuffer, params,
                                  pinsActive, pins);
    applyConfig();
}

void OctoWS2811::applyConfig() {
    if (driver) {
        driver->setBrightness(brightness);
        driver->setColorBalance(colorBalance);
    }
}

void OctoWS2811::setPixelInternal(uint32_t index, uint8_t red, uint8_t green, uint8_t blue,
                                  uint8_t white) {
    if (!drawBuffer || index >= totalPixels) {
        return;
    }
    const uint8_t bytesPerPixel = hasWhite ? 4 : 3;
    uint8_t comps[4] = {red, green, blue, white};

    uint8_t mappedOrder = (params & 0x3F) < 30 ? (params & 0x3F) : 0;
    uint8_t *dest = drawBuffer + index * bytesPerPixel;
    for (uint8_t i = 0; i < bytesPerPixel; ++i) {
        dest[i] = comps[kColorOrderTable[mappedOrder][i]];
    }
}
