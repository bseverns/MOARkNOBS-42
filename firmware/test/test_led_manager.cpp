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

void setUp(void) {}
void tearDown(void) {}

void setup() {
    UNITY_BEGIN();
    RUN_TEST(test_brightness_and_color);
    UNITY_END();
}

void loop() {}
