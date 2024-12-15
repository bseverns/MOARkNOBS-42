#include "DisplayManager.h"

DisplayManager::DisplayManager(uint8_t i2cAddress, uint16_t width, uint16_t height)
    : _display(width, height, &Wire, -1), _i2cAddress(i2cAddress) {}

void DisplayManager::begin() {
    if (!_display.begin(_i2cAddress)) {
        Serial.println(F("SSD1306 allocation failed"));
        for (;;); // Halt if the display fails to initialize
    }
    _display.clearDisplay();
    _display.display();
}

void DisplayManager::showText(const char *text, bool drawBorder) {
    _display.clearDisplay();
    if (drawBorder) {
        _display.drawRect(0, 0, _display.width(), _display.height(), SSD1306_WHITE);
    }

    _display.setTextSize(1);
    _display.setTextColor(SSD1306_WHITE);
    _display.setCursor(0, 0);
    _display.print(text);
    _display.display();
}

void DisplayManager::showValue(uint8_t value, bool drawBorder) {
    _display.clearDisplay();

    if (drawBorder) {
        _display.drawRect(0, 0, _display.width(), _display.height(), SSD1306_WHITE);
    }

    _display.setTextSize(2);
    _display.setTextColor(SSD1306_WHITE);
    _display.setCursor(0, 0);
    _display.print(value);
    _display.display();
}

void DisplayManager::showMode(const char *mode, bool drawBorder) {
    _display.clearDisplay();

    if (drawBorder) {
        _display.drawRect(0, 0, _display.width(), _display.height(), SSD1306_WHITE);
    }

    _display.setTextSize(2);
    _display.setTextColor(SSD1306_WHITE);
    _display.setCursor(0, 0);
    _display.print(mode);
    _display.display();
}

void DisplayManager::clear() {
    _display.clearDisplay();
    _display.display();
}