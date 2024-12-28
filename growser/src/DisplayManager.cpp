#include "DisplayManager.h"

DisplayManager::DisplayManager(uint8_t i2cAddress, uint8_t cols, uint8_t rows)
    : _lcd(i2cAddress, cols, rows), _cols(cols), _rows(rows), _statusTimeout(0) {}

void DisplayManager::begin() {
    _lcd.init();  // Initialize the LCD
    _lcd.backlight();  // Turn on the backlight (if supported)
    clear();
}

void DisplayManager::showText(const char *text, bool clearDisplay) {
    if (clearDisplay) {
        clear();
    }
    _lcd.setCursor(0, 0);
    _lcd.print(text);
}

void DisplayManager::showValue(uint8_t value, bool clearDisplay) {
    if (clearDisplay) {
        clear();
    }
    _lcd.setCursor(0, 0);
    _lcd.print("Value: ");
    _lcd.print(value);
}

void DisplayManager::showMode(const char *mode, bool clearDisplay) {
    if (clearDisplay) {
        clear();
    }
    _lcd.setCursor(0, 0);
    _lcd.print("Mode:");
    _lcd.setCursor(0, 1);
    _lcd.print(mode);
}

void DisplayManager::clear() {
    _lcd.clear();
}

void DisplayManager::updateDisplay(uint8_t beatPosition, const std::vector<uint8_t>& envelopeLevels, const char* statusMessage, uint8_t activePot, uint8_t activeChannel) {
    clear();
    _lcd.setCursor(0, 0);
    _lcd.print("Beat: ");
    _lcd.print(beatPosition);
    _lcd.setCursor(0, 1);
    _lcd.print("Pot: ");
    _lcd.print(activePot);
    _lcd.print(" Ch: ");
    _lcd.print(activeChannel);
}

void DisplayManager::displayStatus(const char *status, unsigned long duration) {
    _statusMessage = status;
    _statusTimeout = millis() + duration;
    clear();
    _lcd.setCursor(0, 0);
    _lcd.print(status);
}