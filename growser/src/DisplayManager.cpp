#include "DisplayManager.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>


DisplayManager::DisplayManager(uint8_t i2cAddress, uint16_t width, uint16_t height)
    : _display(width, height, &Wire), _i2cAddress(i2cAddress), _statusTimeout(0) {}

void DisplayManager::begin() {
    if (!_display.begin(_i2cAddress)) {
        Serial.println(F("SSD1306 allocation failed"));
        for (;;); // Don't proceed, loop forever
    }
    _display.clearDisplay();
    _display.display();
}

void DisplayManager::showText(const char *text, bool clearDisplay) {
    if (clearDisplay) {
        clear();
    }
    _display.setTextSize(1);         // Normal 1:1 pixel scale
    _display.setTextColor(SSD1306_WHITE); // Draw white text
    _display.setCursor(0, 0);        // Start at top-left corner
    _display.println(text);
    _display.display();
}

void DisplayManager::showValue(uint8_t value, bool clearDisplay) {
    if (clearDisplay) {
        clear();
    }
    _display.setTextSize(1);
    _display.setTextColor(SSD1306_WHITE);
    _display.setCursor(0, 0);
    _display.print("Value: ");
    _display.println(value);
    _display.display();
}

void DisplayManager::showMode(const char *mode, bool clearDisplay) {
    if (clearDisplay) {
        clear();
    }
    _display.setTextSize(1);
    _display.setTextColor(SSD1306_WHITE);
    _display.setCursor(0, 0);
    _display.print("Mode: ");
    _display.println(mode);
    _display.display();
}

void DisplayManager::clear() {
    _display.clearDisplay();
    _display.display();
}

void DisplayManager::updateDisplay(uint8_t beatPosition, const std::vector<uint8_t>& envelopeLevels, const char* statusMessage, uint8_t activePot, uint8_t activeChannel) {
    clear();
    _display.setTextSize(1);
    _display.setTextColor(SSD1306_WHITE);
    _display.setCursor(0, 0);
    _display.print("Beat: ");
    _display.println(beatPosition);
    _display.setCursor(0, 10);
    _display.print("Pot: ");
    _display.print(activePot);
    _display.print(" Ch: ");
    _display.println(activeChannel);
    _display.display();
}

void DisplayManager::displayStatus(const char *status, unsigned long duration) {
    _statusMessage = status;
    _statusTimeout = millis() + duration;
    clear();
    _display.setTextSize(1);
    _display.setTextColor(SSD1306_WHITE);
    _display.setCursor(0, 0);
    _display.println(status);
    _display.display();
}
