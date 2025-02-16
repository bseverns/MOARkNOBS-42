// Display Manager Test for Teensy 4.0
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// I2C Display Settings
#define SCREEN_WIDTH 128  // OLED display width, in pixels
#define SCREEN_HEIGHT 64  // OLED display height, in pixels
#define OLED_RESET    -1  // Reset pin (not used for I2C)
#define SCREEN_ADDRESS 0x3C // I2C address for the display

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

void setup() {
    Serial.begin(9600);

    // Initialize the display
    if (!display.begin(SSD1306_I2C_ADDRESS, SCREEN_ADDRESS)) {
        Serial.println("SSD1306 allocation failed");
        for (;;);
    }

    Serial.println("Display Manager Test Initialized");

    // Clear the buffer
    display.clearDisplay();

    // Display welcome message
    display.setTextSize(1);             // Normal 1:1 pixel scale
    display.setTextColor(SSD1306_WHITE); // Draw white text
    display.setCursor(0, 0);            // Start at top-left corner
    display.println(F("Welcome to the Display Test!"));
    display.display();
    delay(2000);
}

void loop() {
    // Test 1: Draw text
    display.clearDisplay();
    display.setTextSize(2);             // Larger text size
    display.setCursor(0, 0);
    display.println(F("Text Test"));
    display.display();
    delay(2000);

    // Test 2: Draw shapes
    display.clearDisplay();
    display.drawRect(10, 10, 50, 30, SSD1306_WHITE); // Rectangle
    display.fillCircle(80, 30, 15, SSD1306_WHITE);   // Filled circle
    display.display();
    delay(2000);

    // Test 3: Scrolling text
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println(F("Scrolling Test"));
    display.display();
    delay(500);
    display.startscrollleft(0x00, 0x0F); // Scroll all lines left
    delay(2000);
    display.stopscroll();

    // Test 4: Show dynamic values
    for (int i = 0; i <= 100; i += 10) {
        display.clearDisplay();
        display.setCursor(0, 0);
        display.setTextSize(1);
        display.print(F("Dynamic Value: "));
        display.print(i);
        display.display();
        delay(500);
    }

    Serial.println("Display Test Cycle Complete");
    delay(2000); // Pause before repeating
}
