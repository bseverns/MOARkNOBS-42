#ifndef OCTOWS2811_H
#define OCTOWS2811_H

#include <stdint.h>

// Color orderings
#define WS2811_RGB 0
#define WS2811_RBG 1
#define WS2811_GRB 2
#define WS2811_GBR 3
#define WS2811_BRG 4
#define WS2811_BGR 5

// Data rates
#define WS2811_800kHz 0
#define WS2811_400kHz 1
#define WS2813_800kHz 2

class OctoWS2811 {
public:
    OctoWS2811(int numLeds, void* frameBuffer, void* drawBuffer, int config);
    void begin();
    void show();
    void setPixel(uint32_t num, uint8_t r, uint8_t g, uint8_t b);
    void setPixel(uint32_t num, int color);
    int getPixel(uint32_t num) const;
};

#endif // OCTOWS2811_H
