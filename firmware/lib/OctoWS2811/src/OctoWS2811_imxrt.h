/*
 * OctoWS2811 Teensy 4.x backend.
 *
 * Thin wrapper around the ObjectFLED DMA engine that ships with our FastLED fork.
 * ObjectFLED implements the Octo timing model on IMXRT1062 using FlexIO-based DMA.
 */

#pragma once

#include <cstdint>

namespace fl {
class ObjectFLED;
}

class OctoWS2811Driver {
  public:
    OctoWS2811Driver(uint16_t ledsPerStrip, void *drawBuffer, uint8_t config, uint8_t pinCount,
                     const uint8_t *pinList);
    ~OctoWS2811Driver();

    void begin();
    void begin(uint16_t latchMicros);
    void begin(double overclock, uint16_t latchMicros);
    void begin(uint16_t periodNS, uint16_t t0hNS, uint16_t t1hNS, uint16_t latchMicros);

    void show();
    int busy() const;

    void setBrightness(uint8_t value);
    uint8_t getBrightness() const;

    void setColorBalance(uint32_t value);
    uint32_t getColorBalance() const;

  private:
    fl::ObjectFLED *object;
};
