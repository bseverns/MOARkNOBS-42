#include "OctoWS2811_imxrt.h"

#include "../../FastLED/third_party/object_fled/src/ObjectFLED.h"

OctoWS2811Driver::OctoWS2811Driver(uint16_t ledsPerStrip, void *drawBuffer, uint8_t config,
                                   uint8_t pinCount, const uint8_t *pinList)
    : object(nullptr) {
    object = new fl::ObjectFLED(ledsPerStrip, drawBuffer, config, pinCount, pinList, 0);
}

OctoWS2811Driver::~OctoWS2811Driver() { delete object; }

void OctoWS2811Driver::begin() {
    if (object) {
        object->begin();
    }
}

void OctoWS2811Driver::begin(uint16_t latchMicros) {
    if (object) {
        object->begin(latchMicros);
    }
}

void OctoWS2811Driver::begin(double overclock, uint16_t latchMicros) {
    if (object) {
        object->begin(overclock, latchMicros);
    }
}

void OctoWS2811Driver::begin(uint16_t periodNS, uint16_t t0hNS, uint16_t t1hNS,
                             uint16_t latchMicros) {
    if (object) {
        object->begin(periodNS, t0hNS, t1hNS, latchMicros);
    }
}

void OctoWS2811Driver::show() {
    if (object) {
        object->show();
    }
}

int OctoWS2811Driver::busy() const {
    if (!object) {
        return 0;
    }
    return object->busy();
}

void OctoWS2811Driver::setBrightness(uint8_t value) {
    if (object) {
        object->setBrightness(value);
    }
}

uint8_t OctoWS2811Driver::getBrightness() const {
    if (!object) {
        return 255;
    }
    return object->getBrightness();
}

void OctoWS2811Driver::setColorBalance(uint32_t value) {
    if (object) {
        object->setBalance(value);
    }
}

uint32_t OctoWS2811Driver::getColorBalance() const {
    if (!object) {
        return 0xFFFFFF;
    }
    return object->getBalance();
}
