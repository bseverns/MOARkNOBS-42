#pragma once

#include <cstdint>

// Minimal SSD1306 surface used by DisplayManagerStub.cpp and the legacy display
// helpers in Utility.cpp. Hardware builds never place test/ ahead of vendor headers.
class Adafruit_SSD1306 {
  public:
    Adafruit_SSD1306(uint16_t width, uint16_t height, void *)
        : width_(static_cast<int16_t>(width)), height_(static_cast<int16_t>(height)) {}

    void getTextBounds(const char *, int16_t, int16_t, int16_t *x, int16_t *y, uint16_t *width,
                       uint16_t *height) const {
        if (x) *x = 0;
        if (y) *y = 0;
        if (width) *width = 0;
        if (height) *height = 0;
    }

    int16_t width() const { return width_; }
    int16_t height() const { return height_; }
    void clearDisplay() {}
    void setCursor(int16_t, int16_t) {}
    void setTextSize(uint8_t) {}
    void setTextColor(uint16_t) {}
    void display() {}

    template <typename T> void print(const T &) {}
    template <typename T> void println(const T &) {}

  private:
    int16_t width_;
    int16_t height_;
};
