#include "DisplayManager.h"

DisplayManager::DisplayManager(uint8_t clkPin, uint8_t dioPin)
    : _display(clkPin, dioPin) {}

void DisplayManager::begin() {
    _display.setBrightness(0x0F); // Maximum brightness
    clear();
}

void DisplayManager::showText(const char *text) {
    uint8_t segments[2] = {0, 0}; // Array to hold the segment encoding for 2 characters

    // Encode each character into 7-segment format
    for (int i = 0; i < 2 && text[i] != '\0'; i++) {
        segments[i] = encodeCharToSegments(text[i]);
    }

    // Display the encoded segments
    _display.setSegments(segments, 2, 0);
}

void DisplayManager::showValue(uint8_t value) {
    // Display a numeric value
    _display.showNumberDec(value, true); // Show value with leading zeros if needed
}

void DisplayManager::showMode(const char *mode) {
    // Display current mode (e.g., "EF" for envelope follower)
    showText(mode);
}

void DisplayManager::clear() {
    _display.clear();
}

// Helper function to encode characters into 7-segment display format
uint8_t DisplayManager::encodeCharToSegments(char c) {
    switch (c) {
        case '0': return 0b00111111;
        case '1': return 0b00000110;
        case '2': return 0b01011011;
        case '3': return 0b01001111;
        case '4': return 0b01100110;
        case '5': return 0b01101101;
        case '6': return 0b01111101;
        case '7': return 0b00000111;
        case '8': return 0b01111111;
        case '9': return 0b01101111;
        case 'A': return 0b01110111;
        case 'b': return 0b01111100;
        case 'C': return 0b00111001;
        case 'd': return 0b01011110;
        case 'E': return 0b01111001;
        case 'F': return 0b01110001;
        default:  return 0b00000000; // Blank for unsupported characters
    }
}
