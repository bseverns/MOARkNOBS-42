#include <Arduino.h>
#include <unity.h>

// This is the orchestral score for the "full system" PlatformIO environment.
// When you flash teensy40_full_system it runs every high-level integration
// test back-to-back so you can slam through a hardware regression in one go.

void test_long_press_detection();
void corrupt_primary_valid_backup();
void corrupted_primary_and_backup();
void test_eeprom_recovery_after_power_cycle();
void test_calibration_offsets_survive_power_cycle();
void test_brightness_and_color();
void test_update_interval_round_trip();
void test_filter_type_switching();
void test_channel_and_cc();
void test_long_press_requires_confirm();
void test_double_press_ctrl2_cycles_midi_type();

void setup() {
    UNITY_BEGIN();
    RUN_TEST(test_long_press_detection);
    RUN_TEST(test_long_press_requires_confirm);
    RUN_TEST(test_double_press_ctrl2_cycles_midi_type);
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
