#include <Arduino.h>
#include <unity.h>
#include "TestHelpers.h"

void test_brightness_and_color() {
    LEDManager led = createLEDManager();
    led.setBrightness(77);
    TEST_ASSERT_EQUAL_UINT8(77, led.getBrightness());
    CRGB hotpink = CRGB(42, 1, 88);
    led.setColor(hotpink);
    CRGB read = led.getColor();
    TEST_ASSERT_EQUAL_UINT8(hotpink.r, read.r);
    TEST_ASSERT_EQUAL_UINT8(hotpink.g, read.g);
    TEST_ASSERT_EQUAL_UINT8(hotpink.b, read.b);
}
