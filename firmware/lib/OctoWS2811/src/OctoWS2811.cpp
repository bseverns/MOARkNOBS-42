#include "OctoWS2811.h"

#include <algorithm>
#include <cstring>
#include <iterator>

#if defined(__IMXRT1062__)
#include "../FastLED/third_party/object_fled/src/ObjectFLED.h"
#endif

namespace {
constexpr uint8_t kOrderTable[30][4] = {
    {0, 1, 2, 3}, {0, 2, 1, 3}, {1, 0, 2, 3}, {1, 2, 0, 3}, {2, 0, 1, 3},
    {2, 1, 0, 3}, {0, 1, 2, 3}, {0, 2, 1, 3}, {1, 0, 2, 3}, {1, 2, 0, 3},
    {2, 0, 1, 3}, {2, 1, 0, 3}, {3, 0, 1, 2}, {3, 0, 2, 1}, {3, 1, 0, 2},
    {3, 1, 2, 0}, {3, 2, 0, 1}, {3, 2, 1, 0}, {0, 3, 1, 2}, {0, 3, 2, 1},
    {1, 3, 0, 2}, {1, 3, 2, 0}, {2, 3, 0, 1}, {2, 3, 1, 0}, {0, 1, 3, 2},
    {0, 2, 3, 1}, {1, 0, 3, 2}, {1, 2, 3, 0}, {2, 0, 3, 1}, {2, 1, 3, 0},
};

constexpr uint8_t kDefaultPinLayout[OctoWS2811::kDefaultPinCount] = {2, 14, 7, 8, 6, 20, 21, 5};
}

class OctoWS2811::Backend {
public:
#if defined(__IMXRT1062__)
    Backend(uint32_t totalPixels, void *drawBuffer, uint8_t config,
            uint8_t pinCount, const uint8_t *pinList)
        : object(static_cast<uint16_t>(totalPixels), drawBuffer, config, pinCount, pinList, 0) {}

    void begin() { object.begin(); }
    void begin(uint16_t latch) { object.begin(latch); }
    void begin(double overclock, uint16_t latch) { object.begin(overclock, latch); }
    void begin(uint16_t period, uint16_t t0h, uint16_t t1h, uint16_t latch) {
        object.begin(period, t0h, t1h, latch);
    }
    void show() { object.show(); }
    int busy() { return object.busy(); }
    void setBrightness(uint8_t value) { object.setBrightness(value); }
    uint8_t getBrightness() const { return object.getBrightness(); }
    void setBalance(uint32_t value) { object.setBalance(value); }
    uint32_t getBalance() const { return object.getBalance(); }
#else
    Backend(uint32_t, void *, uint8_t, uint8_t, const uint8_t *) {}
    void begin() {}
    void begin(uint16_t) {}
    void begin(double, uint16_t) {}
    void begin(uint16_t, uint16_t, uint16_t, uint16_t) {}
    void show() {}
    int busy() { return 0; }
    void setBrightness(uint8_t) {}
    uint8_t getBrightness() const { return 255; }
    void setBalance(uint32_t) {}
    uint32_t getBalance() const { return 0xFFFFFF; }
#endif

#if defined(__IMXRT1062__)
    fl::ObjectFLED object;
#endif
};

const uint8_t OctoWS2811::defaultPins[OctoWS2811::kDefaultPinCount] = {2, 14, 7, 8, 6, 20, 21, 5};

OctoWS2811::OctoWS2811(uint32_t numPerStrip, void *frameBuffer, void *drawBuffer,
                       uint8_t config)
    : OctoWS2811(numPerStrip, frameBuffer, drawBuffer, config, defaultPins, kDefaultPinCount) {}

OctoWS2811::OctoWS2811(uint32_t numPerStrip, void *frameBuffer, void *drawBuffer,
                       uint8_t config, const uint8_t *pinList, uint8_t pinCount)
    : ledsPerStrip(numPerStrip), totalPixels(numPerStrip * pinCount), params(config),
      hasWhite((config & 0x3F) >= WS2811_RGBW), pinsUsed(pinCount),
      frameBufferBytes(static_cast<uint8_t *>(frameBuffer)),
      drawBufferBytes(static_cast<uint8_t *>(drawBuffer)), pendingBrightness(255),
      pendingBalance(0xFFFFFF), backend(nullptr) {
    if (pinCount == 0) {
        pinsUsed = kDefaultPinCount;
    } else if (pinCount > kDefaultPinCount) {
        pinsUsed = kDefaultPinCount;
    }
    std::fill(std::begin(pins), std::end(pins), 0);
    const uint8_t *source = pinList ? pinList : defaultPins;
    std::memcpy(pins, source, pinsUsed);
    totalPixels = ledsPerStrip * pinsUsed;
}

OctoWS2811::~OctoWS2811() { delete backend; }

void OctoWS2811::begin() {
    initialiseBackend();
    if (backend) {
        backend->begin();
        applyStoredSettings();
    }
}

void OctoWS2811::begin(uint16_t latchMicros) {
    initialiseBackend();
    if (backend) {
        backend->begin(latchMicros);
        applyStoredSettings();
    }
}

void OctoWS2811::begin(double overclock, uint16_t latchMicros) {
    initialiseBackend();
    if (backend) {
        backend->begin(overclock, latchMicros);
        applyStoredSettings();
    }
}

void OctoWS2811::begin(uint16_t periodNS, uint16_t t0hNS, uint16_t t1hNS, uint16_t latchMicros) {
    initialiseBackend();
    if (backend) {
        backend->begin(periodNS, t0hNS, t1hNS, latchMicros);
        applyStoredSettings();
    }
}

void OctoWS2811::show() {
    if (backend) {
        backend->show();
    }
}

int OctoWS2811::busy() {
    return backend ? backend->busy() : 0;
}

void OctoWS2811::setPixel(uint32_t num, uint8_t red, uint8_t green, uint8_t blue) {
    writePixel(num, red, green, blue, 0);
}

void OctoWS2811::setPixel(uint32_t num, uint8_t red, uint8_t green, uint8_t blue, uint8_t white) {
    writePixel(num, red, green, blue, white);
}

void OctoWS2811::setPixel(uint32_t num, uint32_t rgb) {
    uint8_t white = hasWhite ? static_cast<uint8_t>((rgb >> 24) & 0xFF) : 0;
    uint8_t red = static_cast<uint8_t>((rgb >> 16) & 0xFF);
    uint8_t green = static_cast<uint8_t>((rgb >> 8) & 0xFF);
    uint8_t blue = static_cast<uint8_t>(rgb & 0xFF);
    writePixel(num, red, green, blue, white);
}

uint32_t OctoWS2811::getPixel(uint32_t num) const {
    if (!drawBufferBytes || num >= totalPixels) {
        return 0;
    }
    uint32_t order = params & 0x3F;
    if (order >= 30) {
        order = 0;
    }
    const uint8_t *src = drawBufferBytes + num * bytesPerLed();
    uint8_t components[4] = {0, 0, 0, 0};
    for (uint8_t i = 0; i < bytesPerLed(); ++i) {
        components[kOrderTable[order][i]] = src[i];
    }
    uint32_t color = (static_cast<uint32_t>(components[0]) << 16) |
                     (static_cast<uint32_t>(components[1]) << 8) |
                     static_cast<uint32_t>(components[2]);
    if (hasWhite) {
        color |= static_cast<uint32_t>(components[3]) << 24;
    }
    return color;
}

uint8_t *OctoWS2811::getPixels() { return drawBufferBytes; }

uint32_t OctoWS2811::numPixels() const { return totalPixels; }

void OctoWS2811::setBrightness(uint8_t value) {
    pendingBrightness = value;
    if (backend) {
        backend->setBrightness(value);
    }
}

uint8_t OctoWS2811::getBrightness() const {
    return backend ? backend->getBrightness() : pendingBrightness;
}

void OctoWS2811::setColorBalance(uint32_t balance) {
    pendingBalance = balance;
    if (backend) {
        backend->setBalance(balance);
    }
}

uint32_t OctoWS2811::getColorBalance() const {
    return backend ? backend->getBalance() : pendingBalance;
}

void OctoWS2811::setPins(const uint8_t *pinList, uint8_t count) {
    uint8_t newCount = count;
    if (newCount == 0) {
        newCount = kDefaultPinCount;
    } else if (newCount > kDefaultPinCount) {
        newCount = kDefaultPinCount;
    }
    std::fill(std::begin(pins), std::end(pins), 0);
    const uint8_t *source = pinList ? pinList : defaultPins;
    std::memcpy(pins, source, newCount);
    pinsUsed = newCount;
    totalPixels = ledsPerStrip * pinsUsed;
    delete backend;
    backend = nullptr;
}

void OctoWS2811::initialiseBackend() {
    if (backend || !drawBufferBytes) {
        return;
    }
    backend = new Backend(totalPixels, drawBufferBytes, params, pinsUsed, pins);
}

void OctoWS2811::applyStoredSettings() {
    if (!backend) {
        return;
    }
    backend->setBrightness(pendingBrightness);
    backend->setBalance(pendingBalance);
}

void OctoWS2811::writePixel(uint32_t index, uint8_t red, uint8_t green, uint8_t blue, uint8_t white) {
    if (!drawBufferBytes || index >= totalPixels) {
        return;
    }
    uint32_t order = params & 0x3F;
    if (order >= 30) {
        order = 0;
    }
    uint8_t comps[4] = {red, green, blue, white};
    uint8_t *dest = drawBufferBytes + index * bytesPerLed();
    for (uint8_t i = 0; i < bytesPerLed(); ++i) {
        dest[i] = comps[kOrderTable[order][i]];
    }
}

