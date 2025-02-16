// LED Manager Test for Teensy 4.0
#include <Arduino.h>
#include <FastLED.h>

// GPIO Assignment
#define LED_PIN 6  // Pin connected to LEDs
#define NUM_LEDS 42 // Total number of LEDs

CRGB leds[NUM_LEDS];

void setup() {
    Serial.begin(9600);

    // Initialize LED strip
    FastLED.addLeds<WS2812, LED_PIN, GRB>(leds, NUM_LEDS);
    FastLED.clear();
    FastLED.show();

    Serial.println("LED Manager Test Initialized");
}

void loop() {
    // Test 1: Cycle through colors on all LEDs
    for (int i = 0; i < 255; i++) {
        fill_solid(leds, NUM_LEDS, CHSV(i, 255, 255)); // Full-color gradient
        FastLED.show();
        delay(20);
    }

    // Test 2: Turn on one LED at a time
    for (int i = 0; i < NUM_LEDS; i++) {
        FastLED.clear();
        leds[i] = CRGB::Blue;
        FastLED.show();
        delay(100);
    }

    // Test 3: Brightness sweep
    for (int brightness = 0; brightness <= 255; brightness += 5) {
        FastLED.setBrightness(brightness);
        FastLED.show();
        delay(50);
    }

    // Reset brightness and turn off LEDs
    FastLED.setBrightness(128); // Default brightness
    FastLED.clear();
    FastLED.show();

    Serial.println("LED Test Cycle Complete");
    delay(2000); // Pause before repeating the cycle
}
