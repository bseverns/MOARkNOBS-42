#include "DisplayManager.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "ButtonManager.h"

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

void DisplayManager::showText(const char* line1, const char* line2, const char* line3) {
    clear();
    _display.setTextSize(1);
    _display.setTextColor(SSD1306_WHITE);

    // Line 1
    _display.setCursor(0, 0);
    _display.println(line1);

    // Line 2 (if provided)
    if (line2 && line2[0] != '\0') {
        _display.setCursor(0, 10);
        _display.println(line2);
    }

    // Line 3 (if provided)
    if (line3 && line3[0] != '\0') {
        _display.setCursor(0, 20);
        _display.println(line3);
    }

    _display.display();
}


void DisplayManager::showValue(uint8_t value, bool clearDisplay) {
    if (clearDisplay) {
        _display.clearDisplay();
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
        _display.clearDisplay();
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

void DisplayManager::updateDisplay(uint8_t beatPosition, const std::vector<uint8_t>& envelopeLevels, const char* statusMessage, uint8_t activePot, uint8_t activeChannel, const char* envelopeMode) {
    _display.clearDisplay();
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
    _display.setCursor(0, 20);
    _display.print("Mode: ");
    _display.println(envelopeMode);
    _display.display();
}


void DisplayManager::displayStatus(const char *status, unsigned long duration) {
    _statusMessage = status;
    _statusTimeout = millis() + duration;
    _display.clearDisplay();
    _display.setTextSize(1);
    _display.setTextColor(SSD1306_WHITE);
    _display.setCursor(0, 0);
    _display.println(status);
    _display.display();
}

void DisplayManager::updateFromContext(const ButtonManagerContext& context) {
    _display.clearDisplay();

    // Line 1: Active button and MIDI channel
    _display.setCursor(0, 0);
    _display.print("BTN: ");
    _display.print(context.activePot);
    _display.print(" CH: ");
    _display.println(context.activeChannel);

    // Line 2: Envelope mode
    _display.setCursor(0, 10);
    _display.print("EF: ");
    _display.println(context.envelopeFollowMode ? "ON" : "OFF");

    // Line 3: Display current envelope assignment if applicable
    if (context.potToEnvelopeMap.count(context.activePot)) {
        int envelopeIndex = context.potToEnvelopeMap.at(context.activePot);
        _display.setCursor(0, 20);
        _display.print("ENV->POT: ");
        _display.println(envelopeIndex);
    }

    _display.display();
}