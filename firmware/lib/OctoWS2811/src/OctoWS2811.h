/*
 * OctoWS2811 - High Performance WS2811 LED Display Library
 *
 * This vendored copy is a lightweight wrapper that restores the public
 * interface expected by FastLED while delegating the heavy lifting to the
 * ObjectFLED DMA engine that already ships with our FastLED fork. The goal is
 * to mirror the upstream OctoWS2811 API closely enough that existing sketches
 * – including FastLED's COctoWS2811Controller – build without further tweaks.
 *
 * Copyright (c) 2013 Paul Stoffregen, PJRC.COM, LLC
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#pragma once

#include <Arduino.h>

// Colour order flags (lower 6 bits) line up with the upstream OctoWS2811
// definitions. The ObjectFLED backend honours the same numeric encoding.
#define WS2811_RGB   0
#define WS2811_RBG   1
#define WS2811_GRB   2
#define WS2811_GBR   3
#define WS2811_BRG   4
#define WS2811_BGR   5
#define WS2811_RGBW  6
#define WS2811_RBGW  7
#define WS2811_GRBW  8
#define WS2811_GBRW  9
#define WS2811_BRGW  10
#define WS2811_BGRW  11
#define WS2811_WRGB  12
#define WS2811_WRBG  13
#define WS2811_WGRB  14
#define WS2811_WGBR  15
#define WS2811_WBRG  16
#define WS2811_WBGR  17
#define WS2811_RWGB  18
#define WS2811_RWBG  19
#define WS2811_GWRB  20
#define WS2811_GWBR  21
#define WS2811_BWRG  22
#define WS2811_BWGR  23
#define WS2811_RGWB  24
#define WS2811_RBWG  25
#define WS2811_GRWB  26
#define WS2811_GBWR  27
#define WS2811_BRWG  28
#define WS2811_BGWR  29

// Timing configuration flags. These bits match the historical OctoWS2811
// values so that sketches can request classic 800 kHz WS2811 timing.
#define WS2811_800kHz   0x00
#define WS2811_400kHz   0x40
#define WS2813_800kHz   0x80

class OctoWS2811 {
public:
    static const uint8_t kDefaultPinCount = 8;

    OctoWS2811(uint32_t numPerStrip, void *frameBuffer, void *drawBuffer,
               uint8_t config = WS2811_GRB | WS2811_800kHz);
    OctoWS2811(uint32_t numPerStrip, void *frameBuffer, void *drawBuffer,
               uint8_t config, const uint8_t *pinList, uint8_t pinCount = kDefaultPinCount);
    ~OctoWS2811();

    void begin();
    void begin(uint16_t latchMicros);
    void begin(double overclock, uint16_t latchMicros = 300);
    void begin(uint16_t periodNS, uint16_t t0hNS, uint16_t t1hNS, uint16_t latchMicros = 300);

    void show();
    int busy();

    void setPixel(uint32_t num, uint8_t red, uint8_t green, uint8_t blue);
    void setPixel(uint32_t num, uint8_t red, uint8_t green, uint8_t blue, uint8_t white);
    void setPixel(uint32_t num, uint32_t rgb);
    uint32_t getPixel(uint32_t num) const;

    uint8_t *getPixels();
    uint32_t numPixels() const;

    void setBrightness(uint8_t value);
    uint8_t getBrightness() const;

    void setColorBalance(uint32_t balance);
    uint32_t getColorBalance() const;

    void setPins(const uint8_t *pinList, uint8_t count = kDefaultPinCount);
    uint8_t pinCount() const { return pinsUsed; }

private:
    void initialiseBackend();
    void applyStoredSettings();
    void writePixel(uint32_t index, uint8_t red, uint8_t green, uint8_t blue, uint8_t white);
    uint8_t bytesPerLed() const { return hasWhite ? 4 : 3; }

    static const uint8_t defaultPins[kDefaultPinCount];

    uint32_t ledsPerStrip;
    uint32_t totalPixels;
    uint8_t params;
    bool hasWhite;
    uint8_t pins[kDefaultPinCount];
    uint8_t pinsUsed;

    uint8_t *frameBufferBytes;
    uint8_t *drawBufferBytes;

    uint8_t pendingBrightness;
    uint32_t pendingBalance;

    // Forward declaration lives in ObjectFLED. We store a raw pointer because
    // ObjectFLED manages its own DMA buffers and has no lightweight smart
    // pointer hooks.
    class Backend;
    Backend *backend;
};


