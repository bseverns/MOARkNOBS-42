#include <Arduino.h>
#include <unity.h>

void test_long_press_detection();
void corrupt_primary_valid_backup();
void corrupted_primary_and_backup();
void test_eeprom_recovery_after_power_cycle();
void test_calibration_offsets_survive_power_cycle();
void test_brightness_and_color();
void test_update_interval_round_trip();
void test_filter_type_switching();
void test_channel_and_cc();

void setup() {
    UNITY_BEGIN();
    RUN_TEST(test_long_press_detection);
    RUN_TEST(corrupt_primary_valid_backup);
    RUN_TEST(corrupted_primary_and_backup);
    RUN_TEST(test_eeprom_recovery_after_power_cycle);
    RUN_TEST(test_calibration_offsets_survive_power_cycle);
    RUN_TEST(test_brightness_and_color);
    RUN_TEST(test_update_interval_round_trip);
    RUN_TEST(test_filter_type_switching);
    RUN_TEST(test_channel_and_cc);
    UNITY_END();
}

void loop() {}
